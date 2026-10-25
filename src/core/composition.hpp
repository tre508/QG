#pragma once

// CoreServices — the single ownership root for QG's non-UI subsystems
// (DESIGN.md §14.3, §15.1–§15.2, §24).
//
// App constructs one CoreServices and wires it once; the composition test
// constructs the same object graph so the wiring contract is verified without a
// running GUI. Exactly one instance owns each service/controller — no duplicates
// and no second mpv owner (PlaybackManager is the process singleton; its single
// MpvBackend is owned here).

#include <QObject>
#include <QString>
#include <memory>

// Global-namespace types (playback + search + file actions + selection live
// outside `qg`).
class ActionsController;
class FileOperationsService;
class KeyboardFramework;
class MountsController;
class MpvBackend;
class PlaybackContext;
class PlaybackController;
class PlaybackManager;
class ReelModel;
class SearchController;
class SearchProxy;
class SelectionState;
class TrashService;
class TypeToSelectBuffer;

namespace qg {

class BrowserController;
class Database;
class FolderModel;
class MountService;
class ProbeController;
class StateController;
class ThumbnailController;

// Register the composed C++ types under the `qg.core` QML module so QML can
    // reference them (review.md D-19). Idempotent; safe to call more than once.
    void registerCoreQmlTypes();

    class CoreServices : public QObject {
        Q_OBJECT

    public:
        explicit CoreServices(QObject *parent = nullptr);
        ~CoreServices() override;

        CoreServices(const CoreServices &) = delete;
        CoreServices &operator=(const CoreServices &) = delete;

        // Construct-once + dependency wiring. `database` may be null (QG still
        // browses and plays without persistence; DESIGN §21 best-effort).
        // `browser`/`mounts` must be non-null when called from App.
        void wire(Database *database,
                  BrowserController *browser,
                  MountService *mounts,
                  const QString &qgCacheDir,
                  const QString &freedesktopThumbHome);

        bool isWired() const { return m_wired; }

        // Owned subsystems (valid after wire()).
        StateController *stateController() const { return m_state.get(); }
        ThumbnailController *thumbnailController() const { return m_thumbnail.get(); }
        ProbeController *probeController() const { return m_probe.get(); }
        SearchProxy *searchProxy() const { return m_searchProxy.get(); }
        SearchController *searchController() const { return m_search.get(); }
        FileOperationsService *fileOperations() const { return m_fileOps.get(); }
        TrashService *trashService() const { return m_trash.get(); }
        ActionsController *actionsController() const { return m_actions.get(); }
        MountsController *mountsController() const { return m_mountsCtl.get(); }
        ReelModel *reelModel() const { return m_reel.get(); }
        SelectionState *selectionState() const { return m_selection.get(); }
        TypeToSelectBuffer *typeToSelectBuffer() const { return m_typeToSelect.get(); }
        KeyboardFramework *keyboardFramework() const { return m_keyboard.get(); }

        // Playback stack (one handle policy).
        MpvBackend *mpvBackend() const { return m_mpv.get(); }
        PlaybackManager *playbackManager() const { return m_playbackManager; }
        PlaybackContext *playbackContext() const { return m_playbackContext.get(); }
        PlaybackController *playbackController() const { return m_playbackController.get(); }

    private:
        void connectMountDisconnectTracking(MountService *mounts);

        std::unique_ptr<StateController> m_state;
        std::unique_ptr<ThumbnailController> m_thumbnail;
        std::unique_ptr<ProbeController> m_probe;
        std::unique_ptr<SearchProxy> m_searchProxy;
        std::unique_ptr<SearchController> m_search;
        std::unique_ptr<FileOperationsService> m_fileOps;
        std::unique_ptr<TrashService> m_trash;
        std::unique_ptr<ActionsController> m_actions;
        std::unique_ptr<MountsController> m_mountsCtl;
        std::unique_ptr<ReelModel> m_reel;
        std::unique_ptr<SelectionState> m_selection;
        std::unique_ptr<TypeToSelectBuffer> m_typeToSelect;
        std::unique_ptr<KeyboardFramework> m_keyboard;

        std::unique_ptr<MpvBackend> m_mpv;
        PlaybackManager *m_playbackManager = nullptr; // process singleton, not owned
        std::unique_ptr<PlaybackContext> m_playbackContext;
        std::unique_ptr<PlaybackController> m_playbackController;

        bool m_wired = false;
    };

} // namespace qg