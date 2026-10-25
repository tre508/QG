#include "app.hpp"

#include "logging.hpp"

#include "controllers/actions_controller.hpp"
#include "controllers/browser_controller.hpp"
#include "controllers/keyboard_framework.hpp"
#include "controllers/mounts_controller.hpp"
#include "controllers/playback_controller.hpp"
#include "controllers/probe_controller.hpp"
#include "controllers/search_controller.hpp"
#include "controllers/state_controller.hpp"
#include "controllers/thumbnail_controller.hpp"
#include "core/composition.hpp"
#include "core/types.hpp"
#include "filesystem/mounts.hpp"
#include "filesystem/operations.hpp"
#include "filesystem/trash.hpp"
#include "models/folder_model.hpp"
#include "models/places_model.hpp"
#include "models/reel_model.hpp"
#include "models/search_proxy.hpp"
#include "models/selection_state.hpp"
#include "persistence/database.hpp"
#include "persistence/repositories.hpp"
#include "playback/mpv_backend.hpp"
#include "playback/playback_context.hpp"
#include "playback/playback_manager.hpp"
#include "playback/playback_session.hpp"

#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QSGRendererInterface>
#include <QStandardPaths>
#include <QVariant>
#include <QtQml/qqml.h>

namespace qg {
namespace {

const char *graphicsApiName(QSGRendererInterface::GraphicsApi api)
{
    switch (api) {
    case QSGRendererInterface::Software:
        return "Software";
    case QSGRendererInterface::OpenGL:
        return "OpenGL";
    case QSGRendererInterface::Direct3D11:
        return "Direct3D11";
    case QSGRendererInterface::Direct3D12:
        return "Direct3D12";
    case QSGRendererInterface::Vulkan:
        return "Vulkan";
    case QSGRendererInterface::Metal:
        return "Metal";
    case QSGRendererInterface::Null:
        return "Null";
    default:
        return "Unknown";
    }
}

bool ensureDirectory(const QString &path)
{
    if (QDir().mkpath(path)) {
        return true;
    }
    qCritical("Failed to create directory: %s", qUtf8Printable(path));
    return false;
}

QString firstExistingDirectory(const QStringList &candidates)
{
    for (const QString &candidate : candidates) {
        if (!candidate.isEmpty() && QFileInfo(candidate).isDir()) {
            return QDir::cleanPath(candidate);
        }
    }
    return {};
}

} // namespace

App::App() = default;

App::~App() = default;

void App::registerQmlTypes()
{
    // Delegates to the composition module so tests can verify the same surface
    // without linking the executable (review.md D-19).
    registerCoreQmlTypes();
}

bool App::initialize()
{
    m_config = Config::fromEnvironment();

    // QG-owned directories under the XDG base directories.
    if (!ensureDirectory(m_config.dataDir()) || !ensureDirectory(m_config.cacheDir())
        || !ensureDirectory(m_config.stateDir()) || !ensureDirectory(m_config.configDir())) {
        return false;
    }

    const bool loggingOk = logging::initialize(m_config.logFilePath());
    if (loggingOk) {
        qInfo("QG starting (Qt %s)", qVersion());
        qInfo("XDG data=%s cache=%s state=%s config=%s",
              qUtf8Printable(m_config.dataDir()),
              qUtf8Printable(m_config.cacheDir()),
              qUtf8Printable(m_config.stateDir()),
              qUtf8Printable(m_config.configDir()));
        qInfo("Qt Quick graphics API: %s",
              graphicsApiName(QQuickWindow::graphicsApi()));
    } else {
        qWarning("QG logging file could not be opened; using stderr only");
    }

    registerCoreTypes();
    registerQmlTypes();

    // --- Persistence (best-effort: QG still browses without a database). -----
    m_database = std::make_unique<Database>();
    const Result<void> opened = m_database->open(m_config.databasePath());
    if (!opened) {
        qWarning("QG database unavailable (%s): %s",
                 qUtf8Printable(m_config.databasePath()),
                 qUtf8Printable(opened.error().message));
        m_database.reset();
    }
    if (m_database) {
        m_recent = std::make_unique<RecentRepository>(*m_database);
        m_settings = std::make_unique<SettingsRepository>(*m_database);
        m_bookmarks = std::make_unique<BookmarkRepository>(*m_database);
    }

    // --- Browser + recent locations + per-folder settings. ------------------
    m_browser = std::make_unique<BrowserController>();
    if (m_recent) {
        RecentRepository *recent = m_recent.get();
        m_browser->setRecentRecorder([recent](const QString &path) { recent->remember(path); });
    }
    if (m_settings) {
        m_browser->setSettings(m_settings.get());
    }

    // --- Places + mounts. ---------------------------------------------------
    m_places = std::make_unique<PlacesModel>();
    if (m_recent) {
        m_places->setRecent(m_recent->recent(10));
    }
    if (m_bookmarks) {
        m_places->setPinned(m_bookmarks->paths());
    }

    // Pinned locations (DESIGN.md §3.3): the SQLite BookmarkRepository is the
    // single source of truth (§21); PlacesModel is a projection that merely asks
    // for the change, so a pin survives a restart and no second list can drift.
    QObject::connect(m_places.get(), &PlacesModel::pinRequested, m_places.get(),
                     [this](const QString &path) {
                         if (!m_bookmarks || path.isEmpty()) {
                             return;
                         }
                         m_bookmarks->add(path, QFileInfo(path).fileName());
                         m_places->setPinned(m_bookmarks->paths());
                     });
    QObject::connect(m_places.get(), &PlacesModel::unpinRequested, m_places.get(),
                     [this](const QString &path) {
                         if (!m_bookmarks || path.isEmpty()) {
                             return;
                         }
                         // Removing the pin drops only the bookmark; the folder
                         // itself is never touched.
                         m_bookmarks->remove(path);
                         m_places->setPinned(m_bookmarks->paths());
                     });

    m_mounts = std::make_unique<MountService>();
    // Desktop capability enrichment (GVfs/UDisks2 through `gio`, §17.4): real
    // labels, removable state and eject/unmount capability. Degrades to kernel
    // mount data when the desktop services are absent.
    m_mounts->setGioEnrichmentEnabled(true);
    QObject::connect(m_mounts.get(), &MountService::volumesChanged, m_places.get(),
                     [this](const QList<VolumeInfo> &volumes) {
                         if (m_places) {
                             m_places->setVolumes(volumes);
                         }
                     });
    m_mounts->start();

    // --- Remaining subsystems (review.md D-01): one owner, wired once. ------
    // CoreServices owns state, thumbnail/probe wiring, search, file actions,
    // drive-disconnect handling, Reel, selection, and the single mpv backend +
    // playback stack. Declared before m_engine so QML drops its pointers first.
    m_services = std::make_unique<CoreServices>();
    m_services->wire(m_database.get(),
                     m_browser.get(),
                     m_mounts.get(),
                     m_config.cacheDir() + QStringLiteral("/thumbnails"),
                     QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation)
                         + QStringLiteral("/thumbnails"));

    // --- QML engine + wiring (P16). -----------------------------------------
    m_engine = std::make_unique<QQmlApplicationEngine>();
    qmlRegisterType<BrowserController>("qg.core", 1, 0, "BrowserController");
    qmlRegisterType<FolderModel>("qg.core", 1, 0, "FolderModel");

    m_engine->loadFromModule("qg", "Main");
    if (m_engine->rootObjects().isEmpty()) {
        qCritical("Failed to load QML shell: qg/Main.qml");
        return false;
    }

    // Inject the live controllers/models into the shell. The shell declares
    // matching `property var` slots (null by default) so it also loads in tests
    // without an application.
    QObject *root = m_engine->rootObjects().first();
    root->setProperty("browserController",
                      QVariant::fromValue(static_cast<QObject *>(m_browser.get())));
    root->setProperty("folderModel",
                      QVariant::fromValue(static_cast<QObject *>(m_browser->model())));
    root->setProperty("placesModel",
                      QVariant::fromValue(static_cast<QObject *>(m_places.get())));
    root->setProperty("mounts",
                      QVariant::fromValue(static_cast<QObject *>(m_mounts.get())));

    // Remaining subsystems (D-01): expose the CoreServices-owned instances.
    auto expose = [root](const char *name, QObject *obj) {
        if (obj != nullptr) {
            root->setProperty(name, QVariant::fromValue(obj));
        }
    };
    if (m_services && m_services->isWired()) {
        expose("stateController", m_services->stateController());
        expose("thumbnailController", m_services->thumbnailController());
        expose("probeController", m_services->probeController());
        expose("searchProxy", m_services->searchProxy());
        expose("searchController", m_services->searchController());
        expose("actionsController", m_services->actionsController());
        expose("mountsController", m_services->mountsController());
        expose("reelModel", m_services->reelModel());
        expose("selectionState", m_services->selectionState());
        expose("playbackContext", m_services->playbackContext());
        expose("playbackController", m_services->playbackController());
        expose("playbackManager", m_services->playbackManager());
        // Central keyboard router (REMEDIATION-012 / D-11).
        expose("keyboard", m_services->keyboardFramework());
    }

    // Restore the last-used location (or Videos/Home on first run).
    QString startLocation;
    if (m_recent) {
        const QList<QString> recents = m_recent->recent(1);
        if (!recents.isEmpty()) {
            startLocation = recents.first();
        }
    }
    startLocation = firstExistingDirectory(
        {startLocation,
         QStandardPaths::writableLocation(QStandardPaths::MoviesLocation),
         QStandardPaths::writableLocation(QStandardPaths::HomeLocation)});
    if (!startLocation.isEmpty()) {
        m_browser->navigate(startLocation);
    }

    m_initialized = true;
    return true;
}

int App::exec()
{
    if (!m_initialized) {
        qCritical("App::exec() called before a successful initialize()");
        return 1;
    }
    const int code = QGuiApplication::exec();
    if (m_mounts) {
        m_mounts->stop();
    }
    logging::shutdown();
    return code;
}

} // namespace qg
