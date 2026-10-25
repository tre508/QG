#include "controllers/keyboard_framework.hpp"

#include "controllers/browser_controller.hpp"
#include "controllers/playback_controller.hpp"
#include "models/folder_model.hpp"
#include "models/selection_state.hpp"
#include "playback/playback_manager.hpp"
#include "playback/playback_session.hpp"

#include <QLatin1String>

namespace {

// The canonical §13.2 window-scoped map: navigation, view toggles, file actions
// on the focused row, and playback. Membership IS the contract (a test asserts
// exact coverage with no duplicates); the ordering is presentational.
const QStringList &actionTable()
{
    static const QStringList table = {
        QStringLiteral("nav.back"),
        QStringLiteral("nav.forward"),
        QStringLiteral("nav.parent"),
        QStringLiteral("nav.refresh"),
        QStringLiteral("nav.home"),
        QStringLiteral("nav.location"),
        QStringLiteral("view.search"),
        QStringLiteral("view.order"),
        QStringLiteral("view.places"),
        QStringLiteral("view.reel"),
        QStringLiteral("view.density1"),
        QStringLiteral("view.density2"),
        QStringLiteral("view.density3"),
        QStringLiteral("item.open"),
        QStringLiteral("item.rename"),
        QStringLiteral("item.trash"),
        QStringLiteral("item.deletePermanent"),
        QStringLiteral("item.favorite"),
        QStringLiteral("item.watched"),
        QStringLiteral("item.reveal"),
        QStringLiteral("item.properties"),
        QStringLiteral("playback.playPause"),
        QStringLiteral("playback.seekBack"),
        QStringLiteral("playback.seekForward"),
        QStringLiteral("playback.previous"),
        QStringLiteral("playback.next"),
        QStringLiteral("playback.volumeUp"),
        QStringLiteral("playback.volumeDown"),
        QStringLiteral("playback.mute"),
        QStringLiteral("playback.jump0"),
        QStringLiteral("playback.jump1"),
        QStringLiteral("playback.jump2"),
        QStringLiteral("playback.jump3"),
        QStringLiteral("playback.jump4"),
        QStringLiteral("playback.jump5"),
        QStringLiteral("playback.jump6"),
        QStringLiteral("playback.jump7"),
        QStringLiteral("playback.jump8"),
        QStringLiteral("playback.jump9"),
        QStringLiteral("playback.fullscreen"),
        QStringLiteral("playback.stop"),
    };
    return table;
}

constexpr double kSeekStepSeconds = 10.0; // DESIGN §13.2: ← / → / J / L seek ±10s
constexpr double kVolumeStep = 0.05;      // ↑ / ↓ volume nudge (session is 0..1)

} // namespace

KeyboardFramework::KeyboardFramework(QObject *parent) : QObject(parent) {}

QStringList KeyboardFramework::actionIds() const
{
    return actionTable();
}

int KeyboardFramework::focusedRow() const
{
    return selection_ != nullptr ? selection_->focusIndex() : -1;
}

bool KeyboardFramework::dispatch(const QString &actionId)
{
    if (!actionTable().contains(actionId)) {
        return false;
    }

    // --- Navigation (BrowserController). ------------------------------------
    if (actionId == QLatin1String("nav.back")) {
        if (browser_ != nullptr) browser_->back();
    } else if (actionId == QLatin1String("nav.forward")) {
        if (browser_ != nullptr) browser_->forward();
    } else if (actionId == QLatin1String("nav.parent")) {
        if (browser_ != nullptr) browser_->parent();
    } else if (actionId == QLatin1String("nav.refresh")) {
        if (browser_ != nullptr) browser_->refresh();
    } else if (actionId == QLatin1String("nav.home")) {
        if (browser_ != nullptr) browser_->navigate(browser_->homePath());
    } else if (actionId == QLatin1String("nav.location")) {
        emit locationFocusRequested();

    // --- View toggles (shell-owned surfaces). -------------------------------
    } else if (actionId == QLatin1String("view.search")) {
        emit searchFocusRequested();
    } else if (actionId == QLatin1String("view.order")) {
        emit orderMenuRequested();
    } else if (actionId == QLatin1String("view.places")) {
        emit placesToggleRequested();
    } else if (actionId == QLatin1String("view.reel")) {
        emit reelToggleRequested();
    } else if (actionId == QLatin1String("view.density1")) {
        emit densityRequested(0);
    } else if (actionId == QLatin1String("view.density2")) {
        emit densityRequested(1);
    } else if (actionId == QLatin1String("view.density3")) {
        emit densityRequested(2);

    // --- Focused-row file actions (shell owns the editor/dialogs). ----------
    } else if (actionId == QLatin1String("item.open")) {
        emit openRequested(focusedRow());
    } else if (actionId == QLatin1String("item.rename")) {
        emit renameRequested(focusedRow());
    } else if (actionId == QLatin1String("item.trash")) {
        emit trashRequested(focusedRow());
    } else if (actionId == QLatin1String("item.deletePermanent")) {
        emit permanentDeleteRequested(focusedRow());
    } else if (actionId == QLatin1String("item.favorite")) {
        emit favoriteToggleRequested(focusedRow());
    } else if (actionId == QLatin1String("item.watched")) {
        emit watchedToggleRequested(focusedRow());
    } else if (actionId == QLatin1String("item.reveal")) {
        emit revealRequested(focusedRow());
    } else if (actionId == QLatin1String("item.properties")) {
        emit propertiesRequested(focusedRow());

    // --- Playback (one handle; PlaybackManager + PlaybackController). -------
    } else if (actionId == QLatin1String("playback.playPause")) {
        if (playbackManager_ != nullptr) playbackManager_->togglePlayPause();
    } else if (actionId == QLatin1String("playback.seekBack")) {
        if (playbackManager_ != nullptr) playbackManager_->seek(-kSeekStepSeconds);
    } else if (actionId == QLatin1String("playback.seekForward")) {
        if (playbackManager_ != nullptr) playbackManager_->seek(kSeekStepSeconds);
    } else if (actionId == QLatin1String("playback.previous")) {
        if (playbackController_ != nullptr) playbackController_->previous();
    } else if (actionId == QLatin1String("playback.next")) {
        if (playbackController_ != nullptr) playbackController_->next();
    } else if (actionId == QLatin1String("playback.volumeUp")) {
        if (playbackManager_ != nullptr && playbackManager_->session() != nullptr) {
            const double v = playbackManager_->session()->volume();
            playbackManager_->setVolume(v + kVolumeStep > 1.0 ? 1.0 : v + kVolumeStep);
        }
    } else if (actionId == QLatin1String("playback.volumeDown")) {
        if (playbackManager_ != nullptr && playbackManager_->session() != nullptr) {
            const double v = playbackManager_->session()->volume();
            playbackManager_->setVolume(v - kVolumeStep < 0.0 ? 0.0 : v - kVolumeStep);
        }
    } else if (actionId == QLatin1String("playback.mute")) {
        if (playbackManager_ != nullptr && playbackManager_->session() != nullptr) {
            playbackManager_->setMuted(!playbackManager_->session()->muted());
        }
    } else if (actionId.startsWith(QLatin1String("playback.jump"))) {
        // 0–9 → jump to the digit's tenth of the duration (DESIGN §13.2).
        const int digit = actionId.right(1).toInt();
        if (playbackManager_ != nullptr && playbackManager_->session() != nullptr) {
            const double duration = playbackManager_->session()->duration();
            if (duration > 0.0) {
                playbackManager_->seekAbsolute(duration * digit / 10.0);
            }
        }
    } else if (actionId == QLatin1String("playback.fullscreen")) {
        if (playbackController_ != nullptr) playbackController_->toggleCinema();
    } else if (actionId == QLatin1String("playback.stop")) {
        if (playbackController_ != nullptr) playbackController_->stop();
    }

    return true;
}

int KeyboardFramework::focusRestoreIndexFor(const QString &previousPath) const
{
    if (folderModel_ == nullptr) {
        return -1;
    }
    const int count = folderModel_->rowCount();
    if (count <= 0) {
        return -1;
    }
    // Going up: focus the folder we came from (the entry whose path matches).
    if (!previousPath.isEmpty()) {
        for (int row = 0; row < count; ++row) {
            if (folderModel_->pathAt(row) == previousPath) {
                return row;
            }
        }
    }
    // Entering a folder (or no match): focus the first entry.
    return 0;
}
