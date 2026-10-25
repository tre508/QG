#include "playback_manager.hpp"
#include "playback_session.hpp"
#include "mpv_backend.hpp"
#include "video_surface.hpp"
#include <mpv/client.h>
#include <QTimer>
#include <QDebug>

PlaybackManager *PlaybackManager::instance_ = nullptr;

PlaybackManager *PlaybackManager::instance()
{
    if (!instance_)
    {
        instance_ = new PlaybackManager();
    }
    return instance_;
}

PlaybackManager::PlaybackManager(QObject *parent) : QObject(parent)
{
    session_ = new PlaybackSession(this);
    // Periodic position persistence (DESIGN.md §19.6): a 5s tick while a file
    // plays; final writes happen on pause/stop/close/media change.
    tickTimer_ = new QTimer(this);
    tickTimer_->setInterval(5000);
    connect(tickTimer_, &QTimer::timeout, this, &PlaybackManager::persistPosition);
}

PlaybackSession *PlaybackManager::session() const
{
    return session_;
}

void PlaybackManager::setBackend(MpvBackend *backend)
{
    if (backend_ != backend)
    {
        if (backend_)
            backend_->disconnect(this); // detach from the previous backend
        backend_ = backend;
        if (surface_)
            surface_->setBackend(backend_);
        connectBackend();
        emit backendChanged();
        flushPendingLoad();
    }
}

// Route mpv's observed state into the single session (DESIGN.md §19.2).
void PlaybackManager::connectBackend()
{
    MpvBackend *backend = backend_.data();
    if (backend == nullptr)
        return;
    connect(backend, &MpvBackend::propertyChanged,
            this, &PlaybackManager::onMpvPropertyChanged);
    connect(backend, &MpvBackend::fileLoaded,
            this, &PlaybackManager::onMpvFileLoaded);
    connect(backend, &MpvBackend::endFile,
            this, &PlaybackManager::onMpvEndFile);
}

void PlaybackManager::onMpvPropertyChanged(const QString &name, const QVariant &value)
{
    PlaybackSession *s = session_;
    if (name == QLatin1String("duration"))
    {
        const double d = value.toDouble();
        if (d > 0)
            s->setDuration(d);
    }
    else if (name == QLatin1String("time-pos"))
    {
        if (value.isValid())
            s->setPosition(value.toDouble());
    }
    else if (name == QLatin1String("seekable"))
    {
        s->setSeekable(value.toBool());
    }
    else if (name == QLatin1String("pause"))
    {
        const bool paused = value.toBool();
        if (paused && s->state() == PlaybackSession::Playing)
            s->setState(PlaybackSession::Paused);
        else if (!paused && s->state() == PlaybackSession::Paused)
            s->setState(PlaybackSession::Playing);
    }
    else if (name == QLatin1String("eof-reached"))
    {
        // keep-open=yes keeps the last frame; `pause` may flip to yes just
        // before this arrives, so accept any non-terminal state.
        if (value.toBool() && s->state() != PlaybackSession::Idle && s->state() != PlaybackSession::Error && s->state() != PlaybackSession::Ended)
        {
            flushPosition(); // final position at EOF (file still loaded)
            if (tickTimer_)
                tickTimer_->stop();
            s->setState(PlaybackSession::Ended);
        }
    }
    else if (name == QLatin1String("idle-active"))
    {
        // External stop (any mpv command that unloads the file) → Idle.
        if (value.toBool() && !loadPending_ && (s->state() == PlaybackSession::Playing || s->state() == PlaybackSession::Paused || s->state() == PlaybackSession::Ended))
        {
            if (tickTimer_)
                tickTimer_->stop();
            s->setState(PlaybackSession::Idle);
            emit playbackStopped();
        }
    }
    else if (name == QLatin1String("volume"))
    {
        if (value.isValid())
            s->setVolume(value.toDouble() / 100.0);
    }
    else if (name == QLatin1String("mute"))
    {
        s->setMuted(value.toBool());
    }
}

void PlaybackManager::onMpvFileLoaded()
{
    updateSessionFromMpv();
    if (session_->state() != PlaybackSession::Paused)
        session_->setState(PlaybackSession::Playing);
}

void PlaybackManager::onMpvEndFile(int reason, int error)
{
    // mpv_end_file_reason: 0 EOF, 2 STOP, 3 QUIT, 4 ERROR.
    switch (reason)
    {
    case 4: // MPV_END_FILE_REASON_ERROR
        session_->setErrorMessage(error != 0
                                      ? QStringLiteral("This file could not be played.")
                                      : QStringLiteral("Playback error."));
        session_->setState(PlaybackSession::Error);
        break;
    case 2: // MPV_END_FILE_REASON_STOP — explicit stop()/close()
    case 3: // MPV_END_FILE_REASON_QUIT — shutdown
        if (session_->state() != PlaybackSession::Idle)
        {
            session_->setState(PlaybackSession::Idle);
            emit playbackStopped();
        }
        break;
    default:
        break; // EOF is surfaced via eof-reached (keep-open keeps the frame)
    }
}

// Pull the properties the transport needs once a file is loaded.
void PlaybackManager::updateSessionFromMpv()
{
    if (!backend_ || !backend_->handle())
        return;
    mpv_handle *h = backend_->handle();
    double duration = 0;
    if (mpv_get_property(h, "duration", MPV_FORMAT_DOUBLE, &duration) >= 0 && duration > 0)
        session_->setDuration(duration);
    int seekable = 0;
    if (mpv_get_property(h, "seekable", MPV_FORMAT_FLAG, &seekable) >= 0)
        session_->setSeekable(seekable != 0);
    double volume = 0;
    if (mpv_get_property(h, "volume", MPV_FORMAT_DOUBLE, &volume) >= 0)
        session_->setVolume(volume / 100.0);
    int mute = 0;
    if (mpv_get_property(h, "mute", MPV_FORMAT_FLAG, &mute) >= 0)
        session_->setMuted(mute != 0);
}

void PlaybackManager::attachSurface(VideoSurfaceItem *surface)
{
    if (surface_ == surface)
        return;
    surface_ = surface;
    if (surface_)
    {
        surface_->setBackend(backend_);
        // mpv may only be asked to play once its render context exists, so a
        // load requested before the Stage has painted is completed here.
        connect(surface_, &VideoSurfaceItem::renderContextReady,
                this, &PlaybackManager::flushPendingLoad);
    }
    flushPendingLoad();
}

void PlaybackManager::flushPendingLoad()
{
    if (!loadPending_ || !backend_ || !backend_->handle())
        return;
    if (!backend_->hasRenderContext())
        return; // surface has not presented a frame yet
    loadPending_ = false;
    const QString path = pendingPath_;
    pendingPath_.clear();
    loadFile(path, pendingStart_);
}

void PlaybackManager::open(const QString &path, double startPosition)
{
    openWithIdentity(path, QString(), startPosition);
}

void PlaybackManager::openWithIdentity(const QString &path, const QString &identity,
                                       double startPosition)
{
    if (!backend_ || !backend_->handle())
    {
        qWarning() << "PlaybackManager: no backend";
        return;
    }

    // Persist the outgoing session's final position before replacing it.
    flushPosition();

    session_->setIdentity(identity);
    session_->setPath(path);
    session_->setPosition(0);
    session_->setDuration(0);
    session_->setErrorMessage(QString());
    session_->setState(PlaybackSession::Loading);
    emit playbackStarted(path);

    // vo=libmpv aborts the file if no render context exists yet; hold the
    // request until the Stage surface has presented its first frame.
    if (!backend_->hasRenderContext())
    {
        pendingPath_ = path;
        pendingStart_ = startPosition;
        loadPending_ = true;
        return;
    }

    loadFile(path, startPosition);
}

void PlaybackManager::loadFile(const QString &path, double startPos)
{
    const QByteArray pathUtf8 = path.toUtf8();
    const QByteArray startOpt = QByteArrayLiteral("start=") + QByteArray::number(startPos > 0 ? startPos : 0.0, 'f', 3);
    // `start=` is applied by mpv when the file loads, so a resumed position does
    // not race the load (a seek issued right after `loadfile` can be lost — and
    // must not be, DESIGN.md §19.6).
    const char *cmd[] = {"loadfile", pathUtf8.constData(), "replace", "-1",
                         startOpt.constData(), nullptr};
    mpv_command(backend_->handle(), cmd);

    mpv_set_property_string(backend_->handle(), "pause", "no");
    session_->setPosition(startPos > 0 ? startPos : 0);
    session_->setState(PlaybackSession::Playing);
    if (tickTimer_)
        tickTimer_->start();
}

void PlaybackManager::play()
{
    if (!backend_ || !backend_->handle())
        return;
    mpv_set_property_string(backend_->handle(), "pause", "no");
    session_->setState(PlaybackSession::Playing);
    if (tickTimer_)
        tickTimer_->start();
}

void PlaybackManager::pause()
{
    if (!backend_ || !backend_->handle())
        return;
    mpv_set_property_string(backend_->handle(), "pause", "yes");
    session_->setState(PlaybackSession::Paused);
    if (tickTimer_)
        tickTimer_->stop();
    flushPosition(); // pause is a persistence point (DESIGN.md §19.6)
}

void PlaybackManager::togglePlayPause()
{
    if (session_->state() == PlaybackSession::Playing)
        pause();
    else
        play();
}

void PlaybackManager::stop()
{
    loadPending_ = false;
    pendingPath_.clear();
    if (!backend_ || !backend_->handle())
        return;
    flushPosition(); // final write before the file is unloaded (§19.6)
    if (tickTimer_)
        tickTimer_->stop();
    const char *cmd[] = {"stop", nullptr};
    mpv_command(backend_->handle(), cmd);
    session_->setState(PlaybackSession::Idle);
    emit playbackStopped();
}

// Read the live position for rejection recovery (DESIGN.md playback §3.1:
// "reject the seek request, restore the actual position").
double PlaybackManager::actualPosition() const
{
    double pos = 0.0;
    if (backend_ && backend_->handle() && mpv_get_property(backend_->handle(), "time-pos", MPV_FORMAT_DOUBLE, &pos) >= 0)
    {
        return pos;
    }
    return session_->position();
}

void PlaybackManager::seek(double seconds)
{
    if (!backend_ || !backend_->handle())
        return;
    if (!session_->seekable())
    {
        // Non-seekable stream: restore what is really playing and let the UI
        // flash its affordance instead of issuing a command mpv will reject.
        session_->setPosition(actualPosition());
        emit seekRejected();
        return;
    }
    seekAbsolute(session_->position() + seconds);
}

void PlaybackManager::seekAbsolute(double position)
{
    if (!backend_ || !backend_->handle())
        return;
    if (!session_->seekable())
    {
        session_->setPosition(actualPosition());
        emit seekRejected();
        return;
    }

    const double target = position > 0.0 ? position : 0.0;
    // `pos` is a named local: mpv copies the command and reads the argument
    // array dur// Read the live position for rejection recovery (DESIGN.md playback §3.1:
    // "reject the seek request, restore the actual position").
    double PlaybackManager::actualPosition() const
    {
        double pos = 0.0;
        if (backend_ && backend_->handle() && mpv_get_property(backend_->handle(), "time-pos", MPV_FORMAT_DOUBLE, &pos) >= 0)
        {
            return pos;
        }
        return sing the call, so its storage must outlive the call.Passing
                              // QByteArray::number(...).constData() directly (the previous code) left
                              // args[1] dangling before mpv ever saw it.
                              const QByteArray pos = QByteArray::number(target, 'f', 3);
        const char *args[] = {"seek", pos.constData(), "absolute+exact", nullptr};
        // Async: rapid seeks must not block the GUI thread, and the latest request
        // wins because mpv applies commands in order and drops superseded seeks
        // (DESIGN.md playback §3.1.4).
        mpv_command_async(backend_->handle(), 0, args);
        // Optimistic indicator; the next observed time-pos corrects it (§3.1.2/5).
        session_->setPosition(target);
    }

    void PlaybackManager::setVolume(double volume)
    {
        // `volume` is the canonical 0..1 fraction (DESIGN.md §8.3); mpv's property
        // is 0..100.
        double clamped = qBound(0.0, volume, 1.0);
        session_->setVolume(clamped);
        if (!backend_ || !backend_->handle())
            return;
        mpv_set_property(backend_->handle(), "volume", MPV_FORMAT_DOUBLE, &clamped);
    }

    void PlaybackManager::setMuted(bool muted)
    {
        if (!backend_ || !backend_->handle())
            return;
        mpv_set_property_string(backend_->handle(), "mute", muted ? "yes" : "no");
        session_->setMuted(muted);
    }

    void PlaybackManager::close()
    {
        stop();
    }

    void PlaybackManager::persistPosition()
    {
        writePosition(false);
    }

    void PlaybackManager::flushPosition()
    {
        writePosition(true);
    }

    // Read the live position and publish it as positionTick (identity-keyed). The
    // periodic path is throttled to one write per 5s; the final path is forced but
    // deduped so stop()+close() cannot write the same value twice (DESIGN.md §19.6).
    void PlaybackManager::writePosition(bool force)
    {
        if (!backend_ || !backend_->handle())
            return;
        const QString identity = session_->identity();
        if (identity.isEmpty())
            return;

        double pos = 0;
        if (mpv_get_property(backend_->handle(), "time-pos", MPV_FORMAT_DOUBLE, &pos) < 0)
            return; // nothing is loaded — nothing to persist
        session_->setPosition(pos);

        if (!force)
        {
            if (pos - lastTickTime_ < 5.0)
                return; // throttled periodic write
            lastTickTime_ = pos;
        }

        const qint64 ms = static_cast<qint64>(pos * 1000.0 + 0.5);
        if (identity == lastWrittenIdentity_ && ms == lastWrittenMs_)
            return; // no redundant write
        lastWrittenIdentity_ = identity;
        lastWrittenMs_ = ms;
        emit positionTick(identity, pos);
    }

    int PlaybackManager::resumePolicy() const { return resumePolicy_; }
    void PlaybackManager::setResumePolicy(int policy)
    {
        if (policy < 0 || policy > 2)
        {
            resumePolicy_ = 0;
            return;
        }
        resumePolicy_ = policy;
    }

    void PlaybackManager::setPositionLoader(std::function<qint64(const QString &)> loader)
    {
        positionLoader_ = std::move(loader);
    }

    // Stored position in seconds, keyed by identity (0 when unknown). Read through
    // the injected loader so playback has no direct dependency on persistence.
    double PlaybackManager::persistedPositionFor(const QString &identity) const
    {
        if (identity.isEmpty() || !positionLoader_)
            return 0;
        const qint64 ms = positionLoader_(identity);
        return ms > 0 ? static_cast<double>(ms) / 1000.0 : 0.0;
    }

    // Resume policy + eligibility (DESIGN.md §19.6). Unknown duration (<= 0) cannot
    // apply the tail guard, so only the > 5s floor is enforced — the item is
    // otherwise indistinguishable from "long video, position unknown".
    double PlaybackManager::resumeStartFor(const QString &identity, double durationSeconds,
                                           bool watched)
    {
        if (identity.isEmpty() || watched || resumePolicy_ == 2 /*Never*/)
            return 0;

        const double stored = persistedPositionFor(identity);
        if (stored <= kMinResumeSeconds)
            return 0;
        if (durationSeconds > 0 && stored >= durationSeconds - kTailGuardSeconds)
            return 0;

        if (resumePolicy_ == 1 /*Ask*/)
        {
            // A real prompt hook: the stored position is offered and the session
            // starts from the beginning until the answer arrives (applyResume).
            emit resumePromptRequested(identity, stored);
            return 0;
        }
        return stored;
    }

    void PlaybackManager::applyResume(const QString &identity, double positionSeconds)
    {
        if (identity.isEmpty() || positionSeconds <= 0)
            return;
        if (session_->identity() != identity)
            return; // only the live session can be resumed
        if (!backend_ || !backend_->handle())
            return;
        seekAbsolute(positionSeconds);
        session_->setPosition(positionSeconds);
    }