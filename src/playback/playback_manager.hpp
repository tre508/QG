#pragma once
#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QVariant>
#include <functional>
#include <memory>

#include "playback_session.hpp"
#include "mpv_backend.hpp"

class MpvBackend;
class VideoSurfaceItem;
class QTimer;

// PlaybackManager: singleton enforcing one-player policy (DESIGN.md §19.1, §19.4).
// Owns the single PlaybackSession. All playback commands route through here.

class PlaybackManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(PlaybackSession *session READ session NOTIFY sessionChanged)

public:
    static PlaybackManager *instance();

    PlaybackSession *session() const;

    Q_INVOKABLE void open(const QString &path, double startPosition = -1);
    Q_INVOKABLE void openWithIdentity(const QString &path, const QString &identity,
                                      double startPosition = -1);
    Q_INVOKABLE void play();
    Q_INVOKABLE void pause();
    Q_INVOKABLE void togglePlayPause();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void seek(double seconds);
    Q_INVOKABLE void seekAbsolute(double position);
    Q_INVOKABLE void setVolume(double volume);
    Q_INVOKABLE void setMuted(bool muted);
    Q_INVOKABLE void close();

    void setBackend(MpvBackend *backend);
    Q_INVOKABLE void attachSurface(VideoSurfaceItem *surface);
    // The single mpv backend, exposed so the QML VideoSurfaceItem can bind to it.
    Q_PROPERTY(MpvBackend *backend READ backend NOTIFY backendChanged)
    MpvBackend *backend() const { return backend_; }
    // Persisted position. `persistPosition()` is the throttled periodic write
    // (at most every 5s); `flushPosition()` is the final write on
    // pause/stop/close/media change (DESIGN.md §19.6).
    Q_INVOKABLE void persistPosition();
    Q_INVOKABLE void flushPosition();

    // Live position as mpv reports it (seconds); the session value when unknown.
    // Used to restore the real position after a rejected seek (§3.1).
    double actualPosition() const;

    // Absolute seek target normalisation (DESIGN.md playback §3.1, §9):
    //   * NaN → 0 (a comparison against NaN is false, which is also where the
    //     previous `position > 0 ? position : 0` clamp landed);
    //   * negative and -inf → 0;
    //   * +inf → the known duration (else 0), because "end of media" is the only
    //     meaningful target infinity can name; seekAbsolute() refuses +inf when no
    //     duration is known rather than silently seeking to the start;
    //   * clamped to [0, duration] when the duration is known.
    // Pure and static, so the rule can be verified without a backend.
    static double normalizeSeekTarget(double position, double duration);

    // Injected seek dispatch. The production path issues mpv's asynchronous
    // command; tests inject a recorder so the consolidation/accumulation policy
    // can be verified without a render context (the same seam style as
    // MountService::setEnricher / setEjector and setPositionLoader).
    // `mode` is an mpv seek mode; returns an mpv error code (0 on success).
    using SeekDispatcher = std::function<int(double targetSeconds, const QString &mode)>;
    void setSeekDispatcher(SeekDispatcher dispatcher);

    // Rapid-seek consolidation (DESIGN.md playback §3.1.4). A burst is at most one
    // seek command per window, with the newest target flushed when the window
    // closes; the intended position owns the indicator for at most kSeekSettleMs
    // so a burst of relative requests cannot be dragged back by a stale
    // observation (§3.1.2/§3.1.5). Exposed for tests and callers.
    static constexpr int kSeekCoalesceMs = 120;
    static constexpr int kSeekSettleMs = 2000;
    static constexpr double kSeekSettleToleranceSeconds = 0.35;

    // Resume policy (DESIGN.md §19.6): Always(=0, default) / Ask(=1) / Never(=2).
    Q_PROPERTY(int resumePolicy READ resumePolicy WRITE setResumePolicy)
    Q_INVOKABLE int resumePolicy() const;
    Q_INVOKABLE void setResumePolicy(int policy);

    // Stored position for an identity, in seconds (0 when none). Read through an
    // injected loader so playback does not depend on the persistence layer.
    Q_INVOKABLE double persistedPositionFor(const QString &identity) const;
    // Injected by the composition root (StateController/Database-backed).
    void setPositionLoader(std::function<qint64(const QString &identity)> loader);

    // Apply the resume policy + eligibility (DESIGN.md §19.6): resume when the
    // stored position is > 5s and < duration − 10s. Returns the seconds to start
    // at (0 = from the beginning). Ask emits resumePromptRequested() and holds at
    // 0 until the answer arrives via applyResume().
    double resumeStartFor(const QString &identity, double durationSeconds, bool watched);
    // Honour an accepted Ask prompt: seek the live session to `positionSeconds`.
    Q_INVOKABLE void applyResume(const QString &identity, double positionSeconds);

    // Resume thresholds (DESIGN.md §19.6), exposed for tests and callers.
    static constexpr double kMinResumeSeconds = 5.0;
    static constexpr double kTailGuardSeconds = 10.0;

signals:
    void sessionChanged();
    void backendChanged();
    void playbackStarted(const QString &path);
    void playbackStopped();
    // Emitted at most every 5s with the current session identity+position.
    void positionTick(const QString &identity, double positionSeconds);
    // Ask policy: the stored position is available; the UI decides (DESIGN §19.6).
    void resumePromptRequested(const QString &identity, double positionSeconds);
    // A seek was refused because the current media is not seekable (§3.1):
    // the session position has been restored; the UI may flash the affordance.
    void seekRejected();

private:
    explicit PlaybackManager(QObject *parent = nullptr);

    void loadFile(const QString &path, double startPos);
    void updateSessionFromMpv();
    // Read the live position and (optionally force) publish it as positionTick.
    // Deduped so stop()+close() cannot double-write the same value.
    void writePosition(bool force);

    // Mirror mpv's observed properties into the session (DESIGN.md §19.2):
    // the transport reads the session, so EOF/loading/error/external stop and
    // duration/position/seekable must all flow through here.
    void onMpvPropertyChanged(const QString &name, const QVariant &value);
    void onMpvFileLoaded();
    void onMpvEndFile(int reason, int error);

    void connectBackend();

    // --- Seeking (DESIGN.md playback §3.1) ------------------------------------
    // The position a relative request must accumulate onto: the newest *intended*
    // target while a seek is being pursued, else the observed session position.
    double intendedPosition() const;
    // Record a new absolute target: publish it immediately (§3.1.2) and either
    // issue it or hold it for the current consolidation window (§3.1.4).
    void requestSeek(double target);
    // Issue the newest target now and open the consolidation window.
    void flushSeek();
    // Consolidation window elapsed: flush the latest deferred target, if any.
    void onSeekWindowClosed();
    // Hand one absolute target to the injected dispatcher or to mpv.
    int dispatchSeek(double target);
    // Refuse a seek: restore the real position, drop the pending burst, notify
    // the UI (§3.1).
    void rejectSeek();
    // Forget any intended position (media change, stop, backend change).
    void clearSeekIntent();

    static PlaybackManager *instance_;

    // QPointer: PlaybackManager is a process singleton and outlives any one
    // backend, so the pointer must self-null when the backend is destroyed.
    QPointer<MpvBackend> backend_ = nullptr;
    QPointer<VideoSurfaceItem> surface_;
    void flushPendingLoad();

    PlaybackSession *session_ = nullptr;
    QString pendingPath_;
    double pendingStart_ = -1;
    bool loadPending_ = false;
    int resumePolicy_ = 0; // 0 Always, 1 Ask, 2 Never
    double lastTickTime_ = 0;
    // Identity-keyed stored-position lookup (ms), injected by composition.
    std::function<qint64(const QString &)> positionLoader_;
    // Dedupe identical consecutive writes (stop() then close(), etc.).
    QString lastWrittenIdentity_;
    qint64 lastWrittenMs_ = -1;
    QTimer *tickTimer_ = nullptr; // 5s periodic position persistence

    // --- Seek state (DESIGN.md playback §3.1.2/§3.1.4/§3.1.5) -----------------
    SeekDispatcher seekDispatcher_;      // empty → mpv's asynchronous command
    double seekTarget_ = 0.0;            // newest intended absolute position
    bool seekIntent_ = false;            // an intended position is being pursued
    bool seekWindowOpen_ = false;        // a seek was issued inside this window
    double pendingSeekTarget_ = 0.0;     // newest target waiting for the window
    bool seekPending_ = false;           // pendingSeekTarget_ is meaningful
    QTimer *seekCoalesceTimer_ = nullptr; // closes the consolidation window
    // Started when a target is issued: bounds how long the intended position may
    // keep owning the indicator before observed reality takes over (§3.1.5).
    QElapsedTimer seekSettleTimer_;
};