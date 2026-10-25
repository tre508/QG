#include "mounts_controller.hpp"

MountsController::MountsController(QObject* parent) : QObject(parent) {}

bool MountsController::disconnected() const { return disconnected_; }
QString MountsController::disconnectedDrive() const { return disconnectedDrive_; }

void MountsController::handleVolumeGone(const QString& driveName, const QString& mountPoint) {
    disconnected_ = true;
    disconnectedDrive_ = driveName;
    disconnectedMount_ = mountPoint;
    emit disconnectedChanged();
}

void MountsController::handleVolumeBack(const QString& driveName, const QString& mountPoint) {
    Q_UNUSED(driveName);
    Q_UNUSED(mountPoint);
    disconnected_ = false;
    disconnectedDrive_.clear();
    disconnectedMount_.clear();
    emit disconnectedChanged();
}

bool MountsController::isInsideDisconnectedVolume(const QString& path) const {
    if (!disconnected_ || disconnectedMount_.isEmpty()) return false;
    return path == disconnectedMount_ || path.startsWith(disconnectedMount_ + "/");
}