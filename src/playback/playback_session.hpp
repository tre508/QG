#pragma once
#include <QObject>
#include <QString>
#include "core/types.hpp"

// Observable playback session state (DESIGN.md §19.2).
// Owned by PlaybackManager. Exposes state, duration, position, audio properties.

class PlaybackSession : public QObject {
    Q_OBJECT
    // Playback session state exposed to QML (Stage.qml, TransportBar.qml,
    // Main.qml). Every property read from QML must be a Q_PROPERTY; a plain
    // accessor returns `undefined` to QML and triggers "Unable to assign
    // [undefined] to <type>" binding failures (FIX-RUNTIME-002-C/D).
    Q_PROPERTY(QString identity READ identity WRITE setIdentity NOTIFY identityChanged)
    Q_PROPERTY(QString path READ path WRITE setPath NOTIFY pathChanged)
    Q_PROPERTY(double duration READ duration WRITE setDuration NOTIFY durationChanged)
    Q_PROPERTY(double position READ position WRITE setPosition NOTIFY positionChanged)
    Q_PROPERTY(State state READ state WRITE setState NOTIFY stateChanged)
    Q_PROPERTY(double volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged)
    Q_PROPERTY(bool seekable READ seekable WRITE setSeekable NOTIFY seekableChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage WRITE setErrorMessage NOTIFY errorMessageChanged)
public:
    enum State { Idle, Loading, Playing, Paused, Ended, Error };
    Q_ENUM(State)

    explicit PlaybackSession(QObject* parent = nullptr);

    QString identity() const;
    void setIdentity(const QString& id);

    QString path() const;
    void setPath(const QString& p);

    double duration() const;
    void setDuration(double d);

    double position() const;
    void setPosition(double p);

    State state() const;
    void setState(State s);

    double volume() const;
    void setVolume(double v);

    bool muted() const;
    void setMuted(bool m);

    bool seekable() const;
    void setSeekable(bool s);

    // Human-readable failure text (empty when there is no error). Surfaced by
    // the Stage playback-failure state (DESIGN.md §8.1, §12).
    QString errorMessage() const;
    void setErrorMessage(const QString& message);

signals:
    void identityChanged();
    void pathChanged();
    void durationChanged();
    void positionChanged();
    void stateChanged(PlaybackSession::State newState);
    void volumeChanged();
    void mutedChanged();
    void seekableChanged();
    void errorMessageChanged();

private:
    QString identity_;
    QString path_;
    double duration_ = 0;
    double position_ = 0;
    State state_ = Idle;
    double volume_ = 1.0;
    bool muted_ = false;
    bool seekable_ = false;
    QString errorMessage_;
};