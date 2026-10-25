#pragma once
#include <QObject>
#include <QString>

// MountsController (DESIGN.md §17.4, §23.13, §23.14): watches mounted-volume
// disappearance and exposes a disconnected-drive state for browsing/playback.
// Never deletes state for files on a disconnected volume.

class MountsController : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool disconnected READ disconnected NOTIFY disconnectedChanged)
    Q_PROPERTY(QString disconnectedDrive READ disconnectedDrive NOTIFY disconnectedChanged)

public:
    explicit MountsController(QObject* parent = nullptr);

    bool disconnected() const;
    QString disconnectedDrive() const;

    // Call when a watched volume disappears (from the mounts watcher).
    Q_INVOKABLE void handleVolumeGone(const QString& driveName, const QString& mountPoint);
    Q_INVOKABLE void handleVolumeBack(const QString& driveName, const QString& mountPoint);
    Q_INVOKABLE bool isInsideDisconnectedVolume(const QString& path) const;

signals:
    void disconnectedChanged();

private:
    bool disconnected_ = false;
    QString disconnectedDrive_;
    QString disconnectedMount_;
};