#include "core/composition.hpp"

#include "controllers/actions_controller.hpp"
#include "controllers/browser_controller.hpp"
#include "controllers/keyboard_framework.hpp"
#include "controllers/mounts_controller.hpp"
#include "controllers/playback_controller.hpp"
#include "controllers/probe_controller.hpp"
#include "controllers/search_controller.hpp"
#include "controllers/state_controller.hpp"
#include "controllers/thumbnail_controller.hpp"
#include "controllers/type_to_select_buffer.hpp"
#include "filesystem/mounts.hpp"
#include "filesystem/operations.hpp"
#include "filesystem/trash.hpp"
#include "models/folder_model.hpp"
#include "models/reel_model.hpp"
#include "models/recursive_search_model.hpp"
#include "models/search_proxy.hpp"
#include "models/selection_state.hpp"
#include "persistence/database.hpp"
#include "persistence/repositories.hpp"
#include "playback/mpv_backend.hpp"
#include "playback/playback_context.hpp"
#include "playback/playback_manager.hpp"
#include "playback/playback_session.hpp"
#include "playback/video_surface.hpp"

#include <QSet>
#include <QtQml/qqml.h>

namespace qg {

void registerCoreQmlTypes()
{
    // Non-creatable: QG owns the single instances; QML only references them.
    qmlRegisterUncreatableType<StateController>("qg.core", 1, 0, "StateController",
                                                "QG owns the single instance");
    qmlRegisterUncreatableType<ThumbnailController>("qg.core", 1, 0, "ThumbnailController",
                                                    "QG owns the single instance");
    qmlRegisterUncreatableType<ProbeController>("qg.core", 1, 0, "ProbeController",
                                                "QG owns the single instance");
    qmlRegisterUncreatableType<SearchProxy>("qg.core", 1, 0, "SearchProxy",
                                            "QG owns the single instance");
    qmlRegisterUncreatableType<SearchController>("qg.core", 1, 0, "SearchController",
                                                 "QG owns the single instance");
    qmlRegisterUncreatableType<ActionsController>("qg.core", 1, 0, "ActionsController",
                                                  "QG owns the single instance");
    qmlRegisterUncreatableType<MountsController>("qg.core", 1, 0, "MountsController",
                                                 "QG owns the single instance");
    qmlRegisterUncreatableType<ReelModel>("qg.core", 1, 0, "ReelModel",
                                          "QG owns the single instance");
    qmlRegisterUncreatableType<RecursiveSearchModel>("qg.core", 1, 0, "RecursiveSearchModel",
                                                     "QG owns the single instance");
    qmlRegisterUncreatableType<SelectionState>("qg.core", 1, 0, "SelectionState",
                                               "QG owns the single instance");
    qmlRegisterUncreatableType<TypeToSelectBuffer>("qg.core", 1, 0, "TypeToSelectBuffer",
                                                   "QG owns the single instance");
    qmlRegisterUncreatableType<PlaybackManager>("qg.core", 1, 0, "PlaybackManager",
                                                "QG owns the single instance");
    qmlRegisterUncreatableType<PlaybackSession>("qg.core", 1, 0, "PlaybackSession",
                                                "QG owns the single instance");
    qmlRegisterUncreatableType<PlaybackContext>("qg.core", 1, 0, "PlaybackContext",
                                                "QG owns the single instance");
    qmlRegisterUncreatableType<PlaybackController>("qg.core", 1, 0, "PlaybackController",
                                                   "QG owns the single instance");
    qmlRegisterUncreatableType<MpvBackend>("qg.core", 1, 0, "MpvBackend",
                                           "QG owns the single instance");
    qmlRegisterUncreatableType<KeyboardFramework>("qg.core", 1, 0, "KeyboardFramework",
                                                  "QG owns the single instance");
    // Creatable: the Stage instantiates exactly one VideoSurfaceItem and binds
    // it to the single backend (D-06/D-20); QG still owns the mpv handle.
    qmlRegisterType<VideoSurfaceItem>("qg.core", 1, 0, "VideoSurfaceItem");
}

CoreServices::CoreServices(QObject *parent) : QObject(parent) {}

CoreServices::~CoreServices() = default;

void CoreServices::wire(Database *database,
                        BrowserController *browser,
                        MountService *mounts,
                        const QString &qgCacheDir,
                        const QString &freedesktopThumbHome)
{
    if (m_wired) {
        return;
    }
    m_wired = true;

    FolderModel *model = browser != nullptr ? browser->model() : nullptr;

    // --- State / metadata persistence (identity-keyed, DESIGN §21.3). --------
    m_state = std::make_unique<StateController>();
    m_state->setDatabase(database);
    m_state->setModel(model);
    if (database != nullptr && model != nullptr) {
        m_state->loadIntoModel();
    }

    // --- Thumbnail pipeline (DESIGN §18.4). ---------------------------------
    m_thumbnail = std::make_unique<ThumbnailController>();
    m_thumbnail->setModel(model);
    m_thumbnail->setDatabase(database);
    m_thumbnail->setCacheDirectory(qgCacheDir);
    m_thumbnail->setFreedesktopCacheHome(freedesktopThumbHome);

    // --- Probe wiring (DESIGN §18.6): coalesced resort to the browser. ------
    m_probe = std::make_unique<ProbeController>();
    m_probe->setModel(model);
    m_probe->setDatabase(database);
    if (browser != nullptr) {
        QObject::connect(m_probe.get(), &ProbeController::resortRequested,
                         browser, &BrowserController::resort);
    }

    // --- Navigation invalidates in-flight thumbnail/probe work (DESIGN §18.4,
    // §22.5). Entering a folder bumps the processing generation so tasks and
    // results belonging to the previous folder are cancelled and dropped before
    // they can touch the model or cache.
    if (browser != nullptr) {
        QObject::connect(browser, &BrowserController::currentPathChanged,
                         m_thumbnail.get(), [this]() { m_thumbnail->beginGeneration(); });
        QObject::connect(browser, &BrowserController::currentPathChanged,
                         m_probe.get(), [this]() { m_probe->beginGeneration(); });
    }

    // --- Search (DESIGN §7, §20.3): proxy sourced from the canonical model. -
    m_searchProxy = std::make_unique<SearchProxy>();
    if (model != nullptr) {
        m_searchProxy->setSourceModel(model);
    }
    m_search = std::make_unique<SearchController>();
    m_search->setProxy(m_searchProxy.get());
    // The recursive walk roots at the current directory; navigation updates it
    // so a stale walk is cancelled and re-issued for the new location (§20.4).
    if (browser != nullptr) {
        m_search->setSearchRoot(browser->currentPath());
        QObject::connect(browser, &BrowserController::currentPathChanged,
                         m_search.get(), [s = m_search.get()](const QString& path) {
                             s->setSearchRoot(path);
                         });
    }

    // --- File actions (DESIGN §11). -----------------------------------------
    m_fileOps = std::make_unique<FileOperationsService>();
    m_trash = std::make_unique<TrashService>();
    m_actions = std::make_unique<ActionsController>();
    m_actions->setFileOperations(m_fileOps.get());
    m_actions->setTrashService(m_trash.get());

    // --- Drive-disconnect handling (DESIGN §17.4, §23.14). ------------------
    m_mountsCtl = std::make_unique<MountsController>();
    connectMountDisconnectTracking(mounts);

    // --- Ordered projections / selection (DESIGN §10, §5.5). ----------------
    m_reel = std::make_unique<ReelModel>();
    m_selection = std::make_unique<SelectionState>();
    m_typeToSelect = std::make_unique<TypeToSelectBuffer>();

    // --- Playback: one handle, one session (DESIGN §19). --------------------
    m_mpv = std::make_unique<MpvBackend>();
    m_playbackManager = PlaybackManager::instance();
    m_playbackManager->setBackend(m_mpv.get());

    // Stored-position lookup for resume (DESIGN.md §19.6): identity-keyed, read
    // from the database on demand. Playback never links persistence directly.
    m_playbackManager->setPositionLoader([database](const QString& identity) -> qint64 {
        if (database == nullptr || identity.isEmpty())
            return 0;
        return MediaStateRepository(*database).positionMs(identity);
    });

    m_playbackContext = std::make_unique<PlaybackContext>();
    m_playbackController = std::make_unique<PlaybackController>();
    m_playbackController->setPlaybackManager(m_playbackManager);
    m_playbackController->setPlaybackContext(m_playbackContext.get());
    m_playbackController->setFolderModel(model);
    // The Reel is the ordered-context projection (DESIGN.md §10): the controller
    // feeds it and keeps it in sync with the canonical model.
    m_playbackController->setReelModel(m_reel.get());

    // --- Drive disconnect stops playback for the host volume (§23.14, LINUX-07).
    // MountsController stays playback-agnostic; the root owns the cross-layer
    // policy so qg_core never depends on qg_playback.
    if (m_mountsCtl != nullptr && m_playbackController != nullptr) {
        QObject::connect(m_mountsCtl.get(), &MountsController::disconnectedChanged,
                         [ctl = m_mountsCtl.get(), pc = m_playbackController.get()]() {
                             if (!ctl->disconnected() || pc == nullptr) {
                                 return;
                             }
                             PlaybackSession* session = pc->currentSession();
                             if (session != nullptr && session->path().length() > 0) {
                                 if (ctl->isInsideDisconnectedVolume(session->path())) {
                                     pc->stop();
                                 }
                             }
                         });
    }

    // --- Central keyboard routing (DESIGN.md §13.2–§13.3). ------------------
    // One router owns the window-scoped shortcut map; it reaches navigation and
    // playback directly and emits intents for the shell-owned surfaces.
    m_keyboard = std::make_unique<KeyboardFramework>();
    m_keyboard->setBrowserController(browser);
    m_keyboard->setFolderModel(model);
    m_keyboard->setSelectionState(m_selection.get());
    m_keyboard->setPlaybackController(m_playbackController.get());
    m_keyboard->setPlaybackManager(m_playbackManager);

    // Throttled position ticks persist by identity (§19.6).
    QObject::connect(m_playbackManager, &PlaybackManager::positionTick,
                     m_state.get(),
                     [this](const QString &identity, double seconds) {
                         m_state->recordPosition(identity, static_cast<qint64>(seconds * 1000.0));
                     });
}

void CoreServices::connectMountDisconnectTracking(MountService *mounts)
{
    if (mounts == nullptr || m_mountsCtl == nullptr) {
        return;
    }
    // Remember which mount points were present so disappearance can be detected.
    auto known = std::make_shared<QSet<QString>>();
    QObject::connect(mounts, &MountService::volumesChanged, m_mountsCtl.get(),
                     [ctl = m_mountsCtl.get(), known](const QList<VolumeInfo> &volumes) {
                         QSet<QString> present;
                         for (const VolumeInfo &v : volumes) {
                             if (v.connected && !v.mountPoint.isEmpty()) {
                                 present.insert(v.mountPoint);
                             }
                         }
                         // Volumes that vanished since the last notification.
                         for (const QString &gone : *known) {
                             if (!present.contains(gone)) {
                                 ctl->handleVolumeGone(gone, gone);
                             }
                         }
                         // Volumes that reappeared.
                         for (const QString &now : present) {
                             if (!known->contains(now)) {
                                 ctl->handleVolumeBack(now, now);
                             }
                         }
                         *known = present;
                     });
}

} // namespace qg