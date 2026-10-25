#include "playback_session.hpp"

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

QString PlaybackSession::errorMessage() const { return errorMessage_; }
void PlaybackSession::setErrorMessage(const QString& message) {
    if (errorMessage_ != message) { errorMessage_ = message; emit errorMessageChanged(); }
}