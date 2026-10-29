#include "playback_session.hpp"

#include <cmath>

PlaybackSession::PlaybackSession(QObject* parent) : QObject(parent) {}

QString PlaybackSession::identity() const { return identity_; }
void PlaybackSession::setIdentity(const QString& id) {
    if (identity_ != id) { identity_ = id; emit identityChanged(); }
}

QString PlaybackSession::path() const { return path_; }
void PlaybackSession::setPath(const QString& p) {
    if (path_ != p) { path_ = p; emit pathChanged(); }
}

double PlaybackSession::duration() const { return duration_; }
void PlaybackSession::setDuration(double d) {
    if (duration_ != d) { duration_ = d; emit durationChanged(); }
}

double PlaybackSession::position() const { return position_; }
void PlaybackSession::setPosition(double p) {
    if (position_ != p) { position_ = p; emit positionChanged(); }
}

PlaybackSession::State PlaybackSession::state() const { return state_; }
void PlaybackSession::setState(State s) {
    if (state_ != s) { state_ = s; emit stateChanged(s); }
}

double PlaybackSession::volume() const { return volume_; }
void PlaybackSession::setVolume(double v) {
    if (volume_ != v) { volume_ = v; emit volumeChanged(); }
}

bool PlaybackSession::muted() const { return muted_; }
void PlaybackSession::setMuted(bool m) {
    if (muted_ != m) { muted_ = m; emit mutedChanged(); }
}

bool PlaybackSession::seekable() const { return seekable_; }
void PlaybackSession::setSeekable(bool s) {
    if (seekable_ != s) { seekable_ = s; emit seekableChanged(); }
}

double PlaybackSession::bufferedEnd() const { return bufferedEnd_; }
void PlaybackSession::setBufferedEnd(double seconds) {
    // mpv reports -1/NaN when the demuxer has no cache: publish "nothing
    // buffered" instead of a bogus range (§3.1: unknown is shown as unknown).
    const double sane = (seconds > 0.0 && std::isfinite(seconds)) ? seconds : 0.0;
    if (bufferedEnd_ != sane) { bufferedEnd_ = sane; emit bufferedEndChanged(); }
}

QString PlaybackSession::errorMessage() const { return errorMessage_; }
void PlaybackSession::setErrorMessage(const QString& message) {
    if (errorMessage_ != message) { errorMessage_ = message; emit errorMessageChanged(); }
}