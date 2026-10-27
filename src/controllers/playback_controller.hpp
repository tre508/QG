#pragma once
#include <QObject>
#include <QString>
#include <QModelIndex>
#include <QList>

#include "playback/playback_session.hpp"
#include "models/reel_model.hpp"

class PlaybackManager;
class PlaybackContext;
class ReelModel;
namespace qg { class FolderModel; }

// PlaybackController: bridges UI intent to PlaybackManager and owns the ordered
// playback context that drives Previous/Next and the Reel (DESIGN.md §10, §14.3,
// §19.4, §23.8). It keeps the Reel projection in sync with the canonical model
// and defends against model changes leaving a stale index (or crashing).

class PlaybackController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool playing READ playing NOTIFY playingChanged)
    Q_PROPERTY(bool cinemaActive READ cinemaActive NOTIFY cinemaActiveChanged)
    Q_PROPERTY(PlaybackSession* currentSession READ currentSession NOTIFY currentSessionChanged)
    // Playback-context filter (All / Favorites / Unwatched) — DESIGN.md §10.2.
    Q_PROPERTY(int reelFilter READ reelFilter NOTIFY reelFilterChanged)
    Q_PROPERTY(QString positionLabel READ positionLabel NOTIFY contextChanged)
    Q_PROPERTY(bool canGoPrevious READ canGoPrevious NOTIFY contextChanged)
    Q_PROPERTY(bool canGoNext READ canGoNext NOTIFY contextChanged)
    // Continuous playback (DESIGN.md playback §4.6/§25.7): at EOF, advance to
    // the next item in the ordered context without leaving Stage or Cinema.
    Q_PROPERTY(bool autoAdvance READ autoAdvance WRITE setAutoAdvance NOTIFY autoAdvanceChanged)

public:
    explicit PlaybackController(QObject* parent = nullptr);

    bool playing() const;
    bool cinemaActive() const;
    PlaybackSession* currentSession() const;

    void setPlaybackManager(PlaybackManager* mgr);
    void setPlaybackContext(PlaybackContext* ctx);
    void setFolderModel(qg::FolderModel* model);
    void setReelModel(ReelModel* reel);

    Q_INVOKABLE void play(const QString& path, const QString& identity);
    Q_INVOKABLE void playCurrent();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void next();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void enterCinema();
    Q_INVOKABLE void exitCinema();
    // F11/F/double-click toggle (DESIGN.md §9.2): enter when a session is live,
    // otherwise no-op.
    Q_INVOKABLE void toggleCinema();
    // True when a session exists and is not idle (Cinema is only meaningful then).
    Q_INVOKABLE bool hasActiveSession() const;

    // --- Reel / ordered context (DESIGN.md §10.2, §19.4, §23.8) --------------
    int reelFilter() const { return reelFilter_; }
    // 0 = All, 1 = Favorites, 2 = Unwatched. Rebuilds the context under the new
    // filter so the Reel and prev/next traverse the filtered list.
    Q_INVOKABLE void setReelFilter(int filter);
    // Play the context entry at `row` (a click in the Reel list).
    Q_INVOKABLE void selectReelRow(int row);
    Q_INVOKABLE QString currentIdentity() const;
    Q_INVOKABLE int contextCount() const;
    Q_INVOKABLE int contextIndex() const;
    QString positionLabel() const; // "7 / 42", or "" with no context
    bool canGoPrevious() const;
    bool canGoNext() const;

    // Resume (DESIGN.md §19.6): honour an accepted "Ask" prompt by seeking the
    // live session to the stored position.
    Q_INVOKABLE void applyResumeDecision(const QString& identity, bool resume,
                                         double positionSeconds);

    // Auto-advance at EOF (§4.6): when the session reaches Ended, step to the
    // next context entry and play it, preserving Stage/Cinema.
    bool autoAdvance() const;
    void setAutoAdvance(bool enabled);

signals:
    void playingChanged();
    void cinemaActiveChanged();
    void currentSessionChanged();
    void playbackStarted(const QString& path);
    void playbackEnded();
    void reelFilterChanged();
    void contextChanged();
    // Prev/Next hit a boundary — surfaced as an "End of folder" toast (§23.8).
    void endOfFolder();
    void autoAdvanceChanged();

private:
    void refreshContext();
    void refreshReel();
    // Rebuild context + reel after the canonical model changes; drop the cursor
    // and stop playback when the playing file disappears from the model.
    void onModelStructureChanged();
    void onModelDataChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight,
                            const QList<int>& roles);
    // One connection per session acquisition; the session is a process singleton,
    // so a duplicate connection would advance twice per EOF.
    void attachSession();
    // Session state mirror: emit playingChanged so the transport reflects
    // pause/EOF/error/stop, and auto-advance on Ended when enabled.
    void onSessionStateChanged(PlaybackSession::State state);
    ReelEntry reelEntryFor(int contextIndex) const;
    // Probed duration (seconds; <=0 when unknown) and watched state from the
    // canonical model, used for resume eligibility (DESIGN.md §19.6).
    double modelDurationSeconds(const QString& identity) const;
    bool modelWatched(const QString& identity) const;

    PlaybackManager* manager_ = nullptr;
    PlaybackContext* context_ = nullptr;
    qg::FolderModel* folderModel_ = nullptr;
    ReelModel* reelModel_ = nullptr;
    PlaybackSession* currentSession_ = nullptr;
    bool cinemaActive_ = false;
    int reelFilter_ = 0;
    bool autoAdvance_ = true;
};
