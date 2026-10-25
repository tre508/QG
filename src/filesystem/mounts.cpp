#include "filesystem/mounts.hpp"

#include "core/types.hpp"
#include "workers/priority_pool.hpp"
#include "workers/tasks.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSet>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>

#include <memory>

namespace qg {
namespace {

const QSet<QString> &pseudoFilesystems()
{
    static const QSet<QString> kPseudo = {
        QStringLiteral("proc"),        QStringLiteral("sysfs"),      QStringLiteral("tmpfs"),
        QStringLiteral("devtmpfs"),    QStringLiteral("devpts"),     QStringLiteral("cgroup"),
        QStringLiteral("cgroup2"),     QStringLiteral("overlay"),    QStringLiteral("securityfs"),
        QStringLiteral("debugfs"),     QStringLiteral("tracefs"),    QStringLiteral("pstore"),
        QStringLiteral("bpf"),         QStringLiteral("configfs"),   QStringLiteral("fusectl"),
        QStringLiteral("mqueue"),      QStringLiteral("hugetlbfs"),  QStringLiteral("autofs"),
        QStringLiteral("rpc_pipefs"),  QStringLiteral("nsfs"),       QStringLiteral("ramfs"),
        QStringLiteral("efivarfs"),    QStringLiteral("binfmt_misc"),
    };
    return kPseudo;
}

// Mount points owned by the running system. Such a mount is never unmounted by
// QG (safety rule, §3.3/§17.4) and is classified `System`.
const QSet<QString> &systemMountPoints()
{
    static const QSet<QString> kSystem = {
        QStringLiteral("/"),     QStringLiteral("/boot"), QStringLiteral("/boot/efi"),
        QStringLiteral("/usr"),  QStringLiteral("/var"),  QStringLiteral("/home"),
        QStringLiteral("/opt"),  QStringLiteral("/srv"),  QStringLiteral("/etc"),
        QStringLiteral("/root"),
    };
    return kSystem;
}

bool isLoopDevice(const QString &device)
{
    return device.startsWith(QLatin1String("/dev/loop"));
}

bool isOpticalDevice(const QString &device)
{
    return device.startsWith(QLatin1String("/dev/sr"));
}

// Remote filesystems. `fuse.*` is deliberately NOT blanket-treated as network:
// several FUSE mounts are local plumbing (portal documents, gvfsd-fuse).
bool isNetworkFilesystem(const QString &fsType)
{
    return fsType.startsWith(QLatin1String("nfs")) || fsType == QLatin1String("cifs")
           || fsType == QLatin1String("smb3") || fsType == QLatin1String("smbfs")
           || fsType == QLatin1String("afpfs") || fsType.startsWith(QLatin1String("davfs"))
           || fsType == QLatin1String("9p") || fsType == QLatin1String("fuse.sshfs")
           || fsType == QLatin1String("fuse.smbnetfs") || fsType == QLatin1String("fuse.rclone");
}

// Addressable local plumbing that is not user storage.
bool isVirtualPlumbing(const QString &fsType, const QString &mountPoint)
{
    if (fsType == QLatin1String("fuse.portal") || fsType == QLatin1String("fuse.gvfsd-fuse")) {
        return true;
    }
    if (fsType.startsWith(QLatin1String("fuse."))
        && mountPoint.startsWith(QLatin1String("/run/user/"))) {
        return true;
    }
    return fsType.startsWith(QLatin1String("zram"));
}

// Decode the escaping udev uses inside /dev/disk/by-label names
// ("Storage\x201\x20Blue" -> "Storage 1 Blue").
QString decodeEscapedName(const QString &name)
{
    QString out;
    out.reserve(name.size());
    for (int i = 0; i < name.size(); ++i) {
        if (name.at(i) == QLatin1Char('\\') && i + 3 < name.size()
            && name.at(i + 1) == QLatin1Char('x')) {
            bool ok = false;
            const int code = name.mid(i + 2, 2).toInt(&ok, 16);
            if (ok) {
                out.append(QChar(code));
                i += 3;
                continue;
            }
        }
        out.append(name.at(i));
    }
    return out;
}

QString deviceBaseName(const QString &device)
{
    return QFileInfo(device).fileName();
}

QString basenameOrPath(const QString &mountPoint)
{
    const QString base = QFileInfo(mountPoint).fileName();
    return base.isEmpty() ? mountPoint : base;
}

// Whether an action backend exists (GVfs/UDisks2 through `gio`, or an injected
// ejector). Without one, no eject/unmount affordance may be claimed: the design
// exposes those actions only when the platform supports them (§17.4).
void enforceCapabilityBackend(QList<VolumeInfo> &volumes, bool haveBackend)
{
    if (haveBackend) {
        return;
    }
    for (VolumeInfo &volume : volumes) {
        volume.ejectable = false;
        volume.canUnmount = false;
    }
}

} // namespace

QString volumeKindName(VolumeKind kind)
{
    switch (kind) {
    case VolumeKind::System:
        return QStringLiteral("system");
    case VolumeKind::Internal:
        return QStringLiteral("internal");
    case VolumeKind::Removable:
        return QStringLiteral("removable");
    case VolumeKind::Optical:
        return QStringLiteral("optical");
    case VolumeKind::Network:
        return QStringLiteral("network");
    case VolumeKind::Image:
        return QStringLiteral("image");
    case VolumeKind::Virtual:
        return QStringLiteral("virtual");
    }
    return QStringLiteral("internal");
}

bool isPseudoFilesystem(const QString &fsType)
{
    return pseudoFilesystems().contains(fsType);
}

QList<VolumeInfo> parseMountInfo(const QByteArray &content)
{
    QList<VolumeInfo> volumes;

    const QList<QByteArray> lines = content.split('\n');
    for (const QByteArray &rawLine : lines) {
        const QByteArray line = rawLine.trimmed();
        if (line.isEmpty()) {
            continue;
        }

        const int separator = line.indexOf(" - ");
        if (separator < 0) {
            continue;
        }

        const QList<QByteArray> pre = line.left(separator).split(' ');
        const QList<QByteArray> post = line.mid(separator + 3).split(' ');
        if (pre.size() < 5 || post.size() < 2) {
            continue;
        }

        const QString mountPoint = QFile::decodeName(pre.at(4));
        const QString fsType = QString::fromUtf8(post.at(0));
        const QString source = QFile::decodeName(post.at(1));

        if (isPseudoFilesystem(fsType)) {
            continue;
        }

        VolumeInfo volume;
        volume.id = mountPoint;
        volume.mountPoint = mountPoint;
        volume.device = source;
        volume.fsType = fsType;
        volume.label = basenameOrPath(mountPoint);

        // Classification by identity, not by path appearance (§3.3): device
        // source, filesystem type and the role of the mount point decide.
        if (isLoopDevice(source)) {
            volume.kind = VolumeKind::Image; // snap/ISO loopback image, not user storage
        } else if (isOpticalDevice(source)) {
            volume.kind = VolumeKind::Optical;
            volume.removable = true;
            volume.ejectable = true;
        } else if (isVirtualPlumbing(fsType, mountPoint)) {
            volume.kind = VolumeKind::Virtual;
        } else if (isNetworkFilesystem(fsType)) {
            volume.kind = VolumeKind::Network;
            volume.network = true;
        } else if (systemMountPoints().contains(mountPoint)) {
            volume.kind = VolumeKind::System;
            volume.systemManaged = true;
        } else {
            volume.kind = VolumeKind::Internal;
        }

        // No eject/unmount capability is claimed from kernel data alone: those
        // actions are exposed only when the platform reports support (§17.4).
        // Removable/eject capability is filled in by enrichment.
        volumes.append(volume);
    }

    return volumes;
}

void enrichVolumesFromDisk(QList<VolumeInfo> &volumes, const DiskEnrichmentPaths &paths)
{
    if (volumes.isEmpty()) {
        return;
    }

    // Labels: /dev/disk/by-label/<label> -> <device>. Authoritative udev data.
    QHash<QString, QString> labelsByDevice;
    QDir byLabel(paths.byLabelDir);
    if (byLabel.exists()) {
        const QFileInfoList entries =
            byLabel.entryInfoList(QDir::Files | QDir::System | QDir::NoDotAndDotDot | QDir::Hidden);
        for (const QFileInfo &entry : entries) {
            const QString target = entry.symLinkTarget();
            if (target.isEmpty()) {
                continue;
            }
            const QString device = deviceBaseName(target);
            if (!device.isEmpty()) {
                labelsByDevice.insert(device, decodeEscapedName(entry.fileName()));
            }
        }
    }

    for (VolumeInfo &volume : volumes) {
        const QString base = deviceBaseName(volume.device);
        if (base.isEmpty()) {
            continue;
        }

        const auto label = labelsByDevice.constFind(base);
        if (label != labelsByDevice.constEnd() && !label.value().isEmpty()) {
            volume.label = label.value();
        }

        // Physical removability of the disk backing the volume. The `..` hop
        // from a partition resolves through the sysfs symlink to the parent
        // disk directory, which owns `removable`.
        if (volume.kind != VolumeKind::Optical && volume.kind != VolumeKind::Image) {
            QFile removable(QDir(paths.sysBlockDir).filePath(base + QStringLiteral("/../removable")));
            if (removable.open(QIODevice::ReadOnly)) {
                if (removable.readAll().trimmed() == QByteArrayLiteral("1")) {
                    volume.removable = true;
                    if (volume.kind == VolumeKind::Internal) {
                        volume.kind = VolumeKind::Removable;
                    }
                }
            }
        }
    }
}

bool gioMountToolAvailable()
{
    static const bool available = !QStandardPaths::findExecutable(QStringLiteral("gio")).isEmpty();
    return available;
}

QHash<QString, GioMountEntry> parseGioMountList(const QByteArray &content)
{
    QHash<QString, GioMountEntry> result;

    struct DriveState {
        QString device;
        bool removable = false;
        bool canEject = false;
    };

    DriveState drive;
    GioMountEntry pending;
    bool inVolume = false;
    bool inMount = false;
    bool volumeHasMount = false;

    auto flush = [&result, &pending, &volumeHasMount]() {
        if (pending.device.isEmpty() && pending.mountPath.isEmpty() && pending.uri.isEmpty()) {
            pending = GioMountEntry();
            volumeHasMount = false;
            return;
        }
        pending.mounted = volumeHasMount;
        if (!pending.device.isEmpty()) {
            result.insert(pending.device, pending);
        }
        if (!pending.mountPath.isEmpty()) {
            result.insert(pending.mountPath, pending);
        }
        if (!pending.uri.isEmpty() && pending.mountPath.isEmpty()) {
            result.insert(pending.uri, pending);
        }
        pending = GioMountEntry();
        volumeHasMount = false;
    };

    const QList<QByteArray> lines = content.split('\n');
    for (const QByteArray &rawLine : lines) {
        const QString raw = QString::fromUtf8(rawLine);
        const QString trimmed = raw.trimmed();
        if (trimmed.isEmpty()) {
            continue;
        }
        // The monitor prints a tree: Drive at column 0, its Volume at 2 spaces,
        // that Volume's Mount at 4+. A Mount at column 0 is a standalone mount
        // (remote shares) and must not be attributed to the preceding volume.
        const int indent = raw.size() - raw.trimmed().size();

        if (trimmed.startsWith(QLatin1String("Drive(")) && trimmed.contains(QLatin1String("): "))) {
            flush();
            inVolume = false;
            inMount = false;
            drive = DriveState();
            continue;
        }

        if (trimmed.startsWith(QLatin1String("Volume(")) && trimmed.contains(QLatin1String("): "))) {
            flush();
            inVolume = true;
            inMount = false;
            pending.driveDevice = drive.device;
            pending.driveRemovable = drive.removable;
            pending.driveCanEject = drive.canEject;
            continue;
        }

        if (trimmed.startsWith(QLatin1String("Mount(")) && trimmed.contains(QLatin1String("): "))) {
            if (indent == 0) {
                flush(); // standalone mount: its own entry
                inVolume = true;
                inMount = true;
                volumeHasMount = true;
            } else {
                inMount = true;
                volumeHasMount = true;
            }
            const int arrow = trimmed.indexOf(QLatin1String("->"));
            if (arrow >= 0) {
                const QString uri = trimmed.mid(arrow + 2).trimmed();
                if (!uri.isEmpty()) {
                    pending.uri = uri;
                    if (uri.startsWith(QLatin1String("file://"))) {
                        pending.mountPath = QUrl(uri).toLocalFile();
                    } else {
                        const QString scheme = QUrl(uri).scheme();
                        if (!scheme.isEmpty() && scheme != QLatin1String("mtp")
                            && scheme != QLatin1String("gphoto2")) {
                            pending.network = true;
                        }
                    }
                }
            }
            continue;
        }

        // Drive attributes (no current volume).
        if (!inVolume) {
            if (trimmed.startsWith(QLatin1String("is_removable="))) {
                drive.removable = trimmed.mid(13).trimmed() == QLatin1String("1");
            } else if (trimmed.startsWith(QLatin1String("can_eject="))) {
                drive.canEject = trimmed.mid(10).trimmed() == QLatin1String("1");
            } else if (trimmed.startsWith(QLatin1String("unix-device: "))) {
                drive.device = QString(trimmed.mid(14)).remove(QLatin1Char('\''));
            }
            continue;
        }

        // Volume / mount attributes.
        if (trimmed.startsWith(QLatin1String("unix-device: "))) {
            const QString device = QString(trimmed.mid(14)).remove(QLatin1Char('\''));
            if (!inMount) {
                pending.device = device;
            }
            continue;
        }
        if (trimmed.startsWith(QLatin1String("label: "))) {
            pending.label = QString(trimmed.mid(7)).remove(QLatin1Char('\''));
            continue;
        }
        if (trimmed.startsWith(QLatin1String("default_location="))) {
            const QString uri = trimmed.mid(17).trimmed();
            if (uri.startsWith(QLatin1String("file://"))) {
                const QString path = QUrl(uri).toLocalFile();
                if (!path.isEmpty()) {
                    pending.mountPath = path;
                }
            } else if (!uri.isEmpty()) {
                pending.uri = uri;
                const QString scheme = QUrl(uri).scheme();
                if (!scheme.isEmpty() && scheme != QLatin1String("mtp")
                    && scheme != QLatin1String("gphoto2")) {
                    pending.network = true;
                }
            }
            continue;
        }
        if (trimmed.startsWith(QLatin1String("can_unmount="))) {
            if (trimmed.mid(12).trimmed() == QLatin1String("1")) {
                pending.canUnmount = true;
            }
            continue;
        }
        if (trimmed.startsWith(QLatin1String("can_eject="))) {
            if (trimmed.mid(10).trimmed() == QLatin1String("1")) {
                pending.canEject = true;
            }
            continue;
        }
    }
    flush();

    return result;
}

void applyGioMountList(QList<VolumeInfo> &volumes, const QHash<QString, GioMountEntry> &entries)
{
    if (entries.isEmpty()) {
        return;
    }

    for (VolumeInfo &volume : volumes) {
        if (volume.kind == VolumeKind::Image || volume.kind == VolumeKind::Virtual) {
            continue;
        }

        GioMountEntry entry;
        bool found = false;
        if (!volume.device.isEmpty()) {
            const auto byDevice = entries.constFind(volume.device);
            if (byDevice != entries.constEnd()) {
                entry = byDevice.value();
                found = true;
            }
        }
        if (!found && !volume.mountPoint.isEmpty()) {
            const auto byPath = entries.constFind(volume.mountPoint);
            if (byPath != entries.constEnd()) {
                entry = byPath.value();
                found = true;
            }
        }
        if (!found) {
            continue;
        }

        // Desktop-reported capabilities are authoritative for action visibility.
        volume.ejectable = entry.canEject || entry.driveCanEject
                           || volume.kind == VolumeKind::Optical;
        volume.canUnmount = entry.canUnmount;

        if (!entry.label.isEmpty()) {
            volume.label = entry.label;
        }
        if (entry.network) {
            volume.network = true;
            if (volume.kind == VolumeKind::Internal || volume.kind == VolumeKind::System) {
                volume.kind = VolumeKind::Network;
            }
        }
        if (entry.driveRemovable || entry.device.startsWith(QLatin1String("/dev/mmcblk"))
            || entry.driveDevice.startsWith(QLatin1String("/dev/mmcblk"))) {
            volume.removable = true;
            if (volume.kind == VolumeKind::Internal) {
                volume.kind = VolumeKind::Removable;
            }
        }
    }
}

MountService::MountService(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<VolumeInfo>("qg::VolumeInfo");
    qRegisterMetaType<QList<VolumeInfo>>("QList<qg::VolumeInfo>");

    m_pollTimer = new QTimer(this);
    connect(m_pollTimer, &QTimer::timeout, this, &MountService::refresh);
}

void MountService::start(int pollIntervalMs)
{
    m_pollTimer->start(pollIntervalMs > 0 ? pollIntervalMs : 5000);
    refresh();
}

void MountService::stop()
{
    m_pollTimer->stop();
}

void MountService::refresh()
{
    const QString path = m_mountInfoPath.isEmpty() ? QStringLiteral("/proc/self/mountinfo")
                                                   : m_mountInfoPath;
    auto result = std::make_shared<QList<VolumeInfo>>();
    Enricher enricher = m_enricher;
    const DiskEnrichmentPaths enrichmentPaths = m_enrichmentPaths;
    const bool haveEjector = static_cast<bool>(m_ejector);
    const bool useGio = m_gioEnrichment && gioMountToolAvailable();

    // All parsing/enrichment runs off the GUI thread; only `applyVolumes`
    // (delivered queued) touches model-facing state.
    submitTask(WorkerPools::io(),
               TaskPriority::Normal,
               nullptr,
               this,
               [path, result, enricher, enrichmentPaths, haveEjector, useGio]() {
                   QFile file(path);
                   QByteArray content;
                   if (file.open(QIODevice::ReadOnly)) {
                       content = file.readAll();
                   }
                   *result = parseMountInfo(content);
                   enrichVolumesFromDisk(*result, enrichmentPaths);
                   if (useGio) {
                       QProcess gio;
                       gio.start(QStringLiteral("gio"), {QStringLiteral("mount"),
                                                        QStringLiteral("-l"), QStringLiteral("-i")});
                       if (gio.waitForFinished(4000)) {
                           applyGioMountList(*result, parseGioMountList(gio.readAllStandardOutput()));
                       }
                   }
                   enforceCapabilityBackend(*result, haveEjector || useGio);
                   if (enricher) {
                       enricher(*result);
                   }
               },
               [this, result]() { applyVolumes(*result); });
}

void MountService::applyVolumes(QList<VolumeInfo> volumes)
{
    reconcileDisconnected(volumes);
    m_volumes = std::move(volumes);
    emit volumesChanged(m_volumes);
}

void MountService::reconcileDisconnected(QList<VolumeInfo> &volumes)
{
    QSet<QString> present;
    present.reserve(volumes.size());
    for (const VolumeInfo &volume : volumes) {
        present.insert(volume.id);
    }

    // Volumes that came back are no longer disconnected; the rest age out.
    for (auto it = m_retained.begin(); it != m_retained.end();) {
        if (present.contains(it.key())) {
            it = m_retained.erase(it);
            continue;
        }
        --it.value().cyclesLeft;
        if (it.value().cyclesLeft <= 0) {
            it = m_retained.erase(it);
            continue;
        }
        ++it;
    }

    // Retain user-relevant volumes that just vanished (§3.3 amber badge,
    // §17.8 retention). System mounts are not retained: their disappearance
    // means the system is going away, not that the user removed a drive.
    for (const VolumeInfo &previous : m_volumes) {
        if (present.contains(previous.id) || !previous.connected) {
            continue;
        }
        if (previous.systemManaged || previous.kind == VolumeKind::Virtual
            || previous.kind == VolumeKind::Image) {
            continue;
        }
        Retained retained;
        retained.volume = previous;
        retained.volume.connected = false;
        retained.volume.mounted = false;
        retained.volume.ejectable = false;
        retained.volume.canUnmount = false;
        retained.cyclesLeft = m_graceCycles;
        m_retained.insert(previous.id, retained);
    }

    // Deterministic order for the disconnected tail.
    QStringList retainedIds = m_retained.keys();
    retainedIds.sort();
    for (const QString &id : retainedIds) {
        volumes.append(m_retained.value(id).volume);
    }
}

bool MountService::runDeviceAction(const QString &volumeId, bool ejectRequested)
{
    const QString verb = ejectRequested ? QStringLiteral("Eject") : QStringLiteral("Unmount");

    for (const VolumeInfo &volume : m_volumes) {
        if (volume.id != volumeId) {
            continue;
        }
        if (volume.systemManaged || volume.mountPoint == QLatin1String("/")) {
            emit ejectFinished(
                volumeId, false,
                QStringLiteral("%1 is part of the running system and is never unmounted.")
                    .arg(volume.label));
            return true;
        }
        if (!volume.mounted) {
            emit ejectFinished(volumeId, false,
                               QStringLiteral("%1 is not connected.").arg(volume.label));
            return true;
        }
        if (ejectRequested ? !volume.ejectable : !volume.canUnmount) {
            emit ejectFinished(volumeId, false,
                               QStringLiteral("%1 does not support %2 on this system.")
                                   .arg(volume.label, ejectRequested ? QStringLiteral("eject")
                                                                    : QStringLiteral("unmount")));
            return true;
        }
        if (m_ejector) {
            QString error;
            const bool ok = m_ejector(volume, &error);
            emit ejectFinished(volumeId, ok, error);
            return true;
        }
        if (!gioMountToolAvailable()) {
            emit ejectFinished(volumeId, false,
                               QStringLiteral("Eject/unmount is not supported in this environment."));
            return true;
        }

        // Desktop-native operation, performed off the GUI thread: the design
        // delegates eject/unmount to GVfs/UDisks2 (§17.4, §23.13).
        const QString device = volume.device;
        const QString uri = QUrl::fromLocalFile(volume.mountPoint).toString();
        auto outcome = std::make_shared<QPair<bool, QString>>(false, QString());
        submitTask(WorkerPools::io(),
                   TaskPriority::Interactive,
                   nullptr,
                   this,
                   [ejectRequested, device, uri, outcome]() {
                       QProcess gio;
                       QStringList args{QStringLiteral("mount"),
                                        ejectRequested ? QStringLiteral("-e") : QStringLiteral("-u")};
                       args << (ejectRequested ? device : uri);
                       gio.start(QStringLiteral("gio"), args);
                       if (!gio.waitForFinished(15000)) {
                           *outcome = {false, QStringLiteral("The operation timed out.")};
                           return;
                       }
                       const QString error = QString::fromLocal8Bit(gio.readAllStandardError()).trimmed();
                       *outcome = {gio.exitStatus() == QProcess::NormalExit && gio.exitCode() == 0,
                                   error};
                   },
                   [this, volumeId, outcome, verb]() {
                       const bool ok = outcome->first;
                       QString message = outcome->second;
                       if (!ok && message.isEmpty()) {
                           message = QStringLiteral("%1 failed.").arg(verb);
                       }
                       emit ejectFinished(volumeId, ok, ok ? QString() : message);
                       if (ok) {
                           refresh(); // sidebar reflects the new mount state (§23.14)
                       }
                   });
        return true;
    }

    emit ejectFinished(volumeId, false, QStringLiteral("Unknown volume"));
    return true;
}

void MountService::eject(const QString &volumeId)
{
    runDeviceAction(volumeId, true);
}

void MountService::unmount(const QString &volumeId)
{
    runDeviceAction(volumeId, false);
}

} // namespace qg
