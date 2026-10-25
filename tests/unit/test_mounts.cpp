// Mount discovery + classification tests (Prompt 11, extended by FIX-UI-003).

#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "filesystem/mounts.hpp"

using namespace qg;

namespace {

const QByteArray kMountInfo = QByteArrayLiteral(
    "25 1 8:1 / / rw,relatime shared:1 - ext4 /dev/sda1 rw\n"
    "30 25 0:23 / /proc rw,nosuid,nodev shared:9 - proc proc rw\n"
    "31 25 0:24 / /sys rw,nosuid shared:10 - sysfs sysfs rw\n"
    "40 25 0:26 / /tmp rw,nosuid shared:11 - tmpfs tmpfs rw\n"
    "50 25 8:2 / /home rw,relatime shared:2 - ext4 /dev/sda2 rw\n"
    "60 25 8:17 / /media/usb rw,nosuid shared:3 - vfat /dev/sdb1 rw");

const QByteArray kMountInfoNoUsb = QByteArrayLiteral(
    "25 1 8:1 / / rw,relatime shared:1 - ext4 /dev/sda1 rw\n"
    "30 25 0:23 / /proc rw,nosuid,nodev shared:9 - proc proc rw\n"
    "31 25 0:24 / /sys rw,nosuid shared:10 - sysfs sysfs rw\n"
    "40 25 0:26 / /tmp rw,nosuid shared:11 - tmpfs tmpfs rw\n"
    "50 25 8:2 / /home rw,relatime shared:2 - ext4 /dev/sda2 rw");

// Every classification family in one fixture (§3.3 / FIX-UI-003-B).
const QByteArray kClassifiedMountInfo = QByteArrayLiteral(
    "25 1 8:1 / / rw,relatime shared:1 - ext4 /dev/nvme0n1p2 rw\n"
    "26 25 8:2 / /boot/efi rw,relatime shared:2 - vfat /dev/nvme0n1p1 rw\n"
    "27 25 8:3 / /mnt/data rw,relatime shared:3 - ext4 /dev/sdc1 rw\n"
    "28 25 0:40 / /snap/core22/2437 ro,relatime shared:4 - squashfs /dev/loop4 ro\n"
    "29 25 11:0 / /media/cd rom,relatime shared:5 - iso9660 /dev/sr0 ro\n"
    "30 25 0:41 / /mnt/nas rw,relatime shared:6 - nfs4 nas.local:/export rw\n"
    "31 25 0:42 / /run/user/1000/doc rw,relatime shared:7 - fuse.portal portal rw\n"
    "32 25 0:43 / /run/user/1000/gvfs rw,relatime shared:8 - fuse.gvfsd-fuse gvfsd-fuse rw\n"
    "33 25 0:44 / /srv/share rw,relatime shared:9 - cifs //host/share rw");

// Captured `gio mount -l -i` shape (GVfs/UDisks2-backed volume monitor).
const QByteArray kGioMountList = QByteArrayLiteral(
    "Drive(0): Seagate Expansion HDD\n"
    "  Type: GProxyDrive (GProxyVolumeMonitorUDisks2)\n"
    "  ids:\n"
    "   unix-device: '/dev/sda'\n"
    "  is_removable=1\n"
    "  can_eject=0\n"
    "  Volume(0): Expansion\n"
    "    Type: GProxyVolume (GProxyVolumeMonitorUDisks2)\n"
    "    ids:\n"
    "     unix-device: '/dev/sda2'\n"
    "     label: 'Expansion'\n"
    "    can_mount=0\n"
    "    can_eject=0\n"
    "    Mount(0): Expansion -> file:///mnt/storage/expansion\n"
    "      Type: GProxyMount (GProxyVolumeMonitorUDisks2)\n"
    "      default_location=file:///mnt/storage/expansion\n"
    "      can_unmount=1\n"
    "      can_eject=0\n"
    "Drive(1): HL-DT-ST DVDRAM\n"
    "  Type: GProxyDrive (GProxyVolumeMonitorUDisks2)\n"
    "  ids:\n"
    "   unix-device: '/dev/sr0'\n"
    "  is_removable=1\n"
    "  can_eject=1\n"
    "  Volume(0): DVD\n"
    "    Type: GProxyVolume (GProxyVolumeMonitorUDisks2)\n"
    "    ids:\n"
    "     unix-device: '/dev/sr0'\n"
    "     label: 'DVD'\n"
    "    can_eject=1\n"
    "Mount(0): share on nas -> smb://nas/share\n"
    "  Type: GProxyMount (GProxyVolumeMonitorSmb)\n"
    "  default_location=smb://nas/share\n"
    "  can_unmount=1\n"
    "  can_eject=0\n");

} // namespace

// Keeps MountService tests independent of the host's real /dev/disk/by-label
// and /sys/class/block trees.
static DiskEnrichmentPaths emptyEnrichmentPaths(const QString &root)
{
    QDir().mkpath(root + QStringLiteral("/by-label"));
    QDir().mkpath(root + QStringLiteral("/sys/class/block"));
    DiskEnrichmentPaths paths;
    paths.byLabelDir = root + QStringLiteral("/by-label");
    paths.sysBlockDir = root + QStringLiteral("/sys/class/block");
    return paths;
}

class MountsTest : public QObject
{
    Q_OBJECT

private slots:
    void pseudoFilesystemsRecognized();
    void parserFiltersPseudoFilesystems();
    void serviceRefreshReadsFixture();
    void classificationByIdentity();
    void enrichmentFromDiskFixture();
    void gioMountListParsing();
    void gioCapabilitiesAppliedToVolumes();
    void noCapabilityWithoutBackend();
    void disconnectedVolumeRetainedAndReconnected();
    void disconnectedVolumeAgesOut();
    void systemMountsAreNeverActedOn();
    void realHostClassificationReport();
};

void MountsTest::pseudoFilesystemsRecognized()
{
    QVERIFY(isPseudoFilesystem(QStringLiteral("proc")));
    QVERIFY(isPseudoFilesystem(QStringLiteral("sysfs")));
    QVERIFY(isPseudoFilesystem(QStringLiteral("tmpfs")));
    QVERIFY(isPseudoFilesystem(QStringLiteral("overlay")));
    QVERIFY(!isPseudoFilesystem(QStringLiteral("ext4")));
    QVERIFY(!isPseudoFilesystem(QStringLiteral("vfat")));
}

void MountsTest::parserFiltersPseudoFilesystems()
{
    const QList<VolumeInfo> volumes = parseMountInfo(kMountInfo);

    QCOMPARE(volumes.size(), 3); // /, /home, /media/usb

    QStringList mountPoints;
    for (const VolumeInfo &volume : volumes) {
        mountPoints << volume.mountPoint;
    }
    QVERIFY(mountPoints.contains(QStringLiteral("/")));
    QVERIFY(mountPoints.contains(QStringLiteral("/home")));
    QVERIFY(mountPoints.contains(QStringLiteral("/media/usb")));
    QVERIFY(!mountPoints.contains(QStringLiteral("/proc")));
    QVERIFY(!mountPoints.contains(QStringLiteral("/tmp")));

    for (const VolumeInfo &volume : volumes) {
        if (volume.mountPoint == QStringLiteral("/home")) {
            QCOMPARE(volume.fsType, QStringLiteral("ext4"));
            QCOMPARE(volume.device, QStringLiteral("/dev/sda2"));
            QCOMPARE(volume.label, QStringLiteral("home"));
        }
    }
}

void MountsTest::serviceRefreshReadsFixture()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString fixturePath = QDir(dir.path()).filePath(QStringLiteral("mountinfo"));
    QFile file(fixturePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(kMountInfo);
    file.close();

    MountService service;
    service.setMountInfoPath(fixturePath);
    service.setEnrichmentPaths(emptyEnrichmentPaths(dir.path()));

    QSignalSpy changed(&service, &MountService::volumesChanged);
    service.refresh();

    QTRY_COMPARE_WITH_TIMEOUT(changed.count(), 1, 3000);
    QCOMPARE(service.volumes().size(), 3);

    QSignalSpy eject(&service, &MountService::ejectFinished);
    service.eject(QStringLiteral("/media/usb"));
    QCOMPARE(eject.count(), 1);
    QVERIFY(!eject.takeFirst().at(1).toBool()); // unsupported without an ejector
}

void MountsTest::classificationByIdentity()
{
    const QList<VolumeInfo> volumes = parseMountInfo(kClassifiedMountInfo);

    auto find = [&volumes](const QString &mountPoint) -> const VolumeInfo * {
        for (const VolumeInfo &volume : volumes) {
            if (volume.mountPoint == mountPoint) {
                return &volume;
            }
        }
        return nullptr;
    };

    // System-managed root device.
    const VolumeInfo *root = find(QStringLiteral("/"));
    QVERIFY(root != nullptr);
    QCOMPARE(root->kind, VolumeKind::System);
    QVERIFY(root->systemManaged);
    QVERIFY(!root->canUnmount); // never unmounted by QG

    // A system mount of the boot device is still classified, never hidden.
    const VolumeInfo *efi = find(QStringLiteral("/boot/efi"));
    QVERIFY(efi != nullptr);
    QCOMPARE(efi->kind, VolumeKind::System);

    // Plain fixed storage.
    const VolumeInfo *data = find(QStringLiteral("/mnt/data"));
    QVERIFY(data != nullptr);
    QCOMPARE(data->kind, VolumeKind::Internal);
    QVERIFY(!data->removable);

    // Loopback image (snap/squashfs) — not user storage, identified by device.
    const VolumeInfo *snap = find(QStringLiteral("/snap/core22/2437"));
    QVERIFY(snap != nullptr);
    QCOMPARE(snap->kind, VolumeKind::Image);

    // Optical media: removable + ejectable by nature.
    const VolumeInfo *cd = find(QStringLiteral("/media/cd"));
    QVERIFY(cd != nullptr);
    QCOMPARE(cd->kind, VolumeKind::Optical);
    QVERIFY(cd->removable);
    QVERIFY(cd->ejectable);

    // Remote filesystems.
    const VolumeInfo *nas = find(QStringLiteral("/mnt/nas"));
    QVERIFY(nas != nullptr);
    QCOMPARE(nas->kind, VolumeKind::Network);
    QVERIFY(nas->network);
    const VolumeInfo *share = find(QStringLiteral("/srv/share"));
    QVERIFY(share != nullptr);
    QCOMPARE(share->kind, VolumeKind::Network);

    // Desktop plumbing is not a network share and not a browsable device.
    const VolumeInfo *portal = find(QStringLiteral("/run/user/1000/doc"));
    QVERIFY(portal != nullptr);
    QCOMPARE(portal->kind, VolumeKind::Virtual);
    QVERIFY(!portal->network);
    const VolumeInfo *gvfs = find(QStringLiteral("/run/user/1000/gvfs"));
    QVERIFY(gvfs != nullptr);
    QCOMPARE(gvfs->kind, VolumeKind::Virtual);
    QVERIFY(!gvfs->network);

    // No capability is claimed from kernel data alone.
    for (const VolumeInfo &volume : volumes) {
        if (volume.kind != VolumeKind::Optical) {
            QVERIFY(!volume.canUnmount);
            QVERIFY(!volume.ejectable);
        }
    }
}

void MountsTest::enrichmentFromDiskFixture()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString root = dir.path();
    // Faithful sysfs layout: class/block/<partition> is a *symlink* into the
    // device tree, so `..` from the partition lands on the disk directory that
    // owns `removable`.
    QVERIFY(QDir().mkpath(root + QStringLiteral("/dev")));
    QVERIFY(QDir().mkpath(root + QStringLiteral("/by-label")));
    QVERIFY(QDir().mkpath(root + QStringLiteral("/sys/class/block")));
    QVERIFY(QDir().mkpath(root + QStringLiteral("/sys/devices/block/sdb/sdb1")));
    QVERIFY(QFile::link(QStringLiteral("../../devices/block/sdb/sdb1"),
                        root + QStringLiteral("/sys/class/block/sdb1")));

    // A device node stand-in the by-label symlink can point at.
    QFile device(root + QStringLiteral("/dev/sdb1"));
    QVERIFY(device.open(QIODevice::WriteOnly));
    device.close();
    QVERIFY(QFile::link(device.fileName(), root + QStringLiteral("/by-label/Scale\\x20Disk")));

    // The disk directory owns removability.
    QFile removable(root + QStringLiteral("/sys/devices/block/sdb/removable"));
    QVERIFY(removable.open(QIODevice::WriteOnly));
    removable.write("1");
    removable.close();

    QList<VolumeInfo> volumes = parseMountInfo(kMountInfo);
    DiskEnrichmentPaths paths;
    paths.byLabelDir = root + QStringLiteral("/by-label");
    paths.sysBlockDir = root + QStringLiteral("/sys/class/block");
    enrichVolumesFromDisk(volumes, paths);

    bool checked = false;
    for (const VolumeInfo &volume : volumes) {
        if (volume.mountPoint != QStringLiteral("/media/usb")) {
            continue;
        }
        checked = true;
        QCOMPARE(volume.label, QStringLiteral("Scale Disk")); // \x20 decoded
        QVERIFY(volume.removable);                            // from /sys .../removable
        QCOMPARE(volume.kind, VolumeKind::Removable);         // upgraded by authority
    }
    QVERIFY(checked);

    // Volumes with no by-label entry and a non-removable disk keep kernel data.
    for (const VolumeInfo &volume : volumes) {
        if (volume.mountPoint == QStringLiteral("/home")) {
            QCOMPARE(volume.label, QStringLiteral("home"));
            QVERIFY(!volume.removable);
            QCOMPARE(volume.kind, VolumeKind::System);
        }
    }
}

void MountsTest::gioMountListParsing()
{
    const QHash<QString, GioMountEntry> entries = parseGioMountList(kGioMountList);

    const auto byDevice = entries.constFind(QStringLiteral("/dev/sda2"));
    QVERIFY(byDevice != entries.constEnd());
    const GioMountEntry usb = byDevice.value();
    QCOMPARE(usb.label, QStringLiteral("Expansion"));
    QCOMPARE(usb.mountPath, QStringLiteral("/mnt/storage/expansion"));
    QCOMPARE(usb.driveDevice, QStringLiteral("/dev/sda"));
    QVERIFY(usb.driveRemovable);
    QVERIFY(usb.canUnmount);
    QVERIFY(!usb.canEject); // the drive reports can_eject=0
    QVERIFY(usb.mounted);

    // The same entry is reachable by mount path.
    QVERIFY(entries.contains(QStringLiteral("/mnt/storage/expansion")));

    // Optical drive: ejectable.
    const auto optical = entries.constFind(QStringLiteral("/dev/sr0"));
    QVERIFY(optical != entries.constEnd());
    QVERIFY(optical.value().canEject);
    QVERIFY(optical.value().driveRemovable);

    // Remote share: no local device, network scheme, unmountable.
    const auto remote = entries.constFind(QStringLiteral("smb://nas/share"));
    QVERIFY(remote != entries.constEnd());
    QVERIFY(remote.value().network);
    QVERIFY(remote.value().canUnmount);
    QVERIFY(!remote.value().canEject);
}

void MountsTest::gioCapabilitiesAppliedToVolumes()
{
    // mountinfo whose device matches the captured GVfs/UDisks2 fixture above.
    const QByteArray mountInfo = QByteArrayLiteral(
        "25 1 8:1 / / rw,relatime shared:1 - ext4 /dev/sda1 rw\n"
        "60 25 8:2 / /mnt/storage/expansion rw,nosuid shared:2 - exfat /dev/sda2 rw");

    QList<VolumeInfo> volumes = parseMountInfo(mountInfo);
    applyGioMountList(volumes, parseGioMountList(kGioMountList));

    bool checked = false;
    for (const VolumeInfo &volume : volumes) {
        if (volume.mountPoint != QStringLiteral("/mnt/storage/expansion")) {
            continue;
        }
        checked = true;
        QCOMPARE(volume.label, QStringLiteral("Expansion"));
        QVERIFY(volume.removable);                        // UDisks2 drive is_removable
        QCOMPARE(volume.kind, VolumeKind::Removable);     // upgraded by authority
        QVERIFY(volume.canUnmount);                       // desktop reports can_unmount=1
        QVERIFY(!volume.ejectable);                       // ... and can_eject=0
    }
    QVERIFY(checked);

    // The root volume is unknown to the desktop monitor and keeps kernel data
    // with no claimed capability.
    for (const VolumeInfo &volume : volumes) {
        if (volume.mountPoint == QStringLiteral("/")) {
            QCOMPARE(volume.kind, VolumeKind::System);
            QVERIFY(volume.systemManaged);
            QVERIFY(!volume.canUnmount);
            QVERIFY(!volume.ejectable);
        }
    }
}

void MountsTest::noCapabilityWithoutBackend()
{
    // Without an ejector and without GVfs/UDisks2 enrichment, QG must not offer
    // an eject/unmount affordance it cannot honour (DESIGN.md §17.4).
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString fixturePath = QDir(dir.path()).filePath(QStringLiteral("mountinfo"));
    QFile file(fixturePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(kClassifiedMountInfo);
    file.close();

    MountService service;
    service.setMountInfoPath(fixturePath);
    service.setEnrichmentPaths(emptyEnrichmentPaths(dir.path()));
    QVERIFY(!service.gioEnrichmentEnabled());

    QSignalSpy changed(&service, &MountService::volumesChanged);
    service.refresh();
    QTRY_COMPARE_WITH_TIMEOUT(changed.count(), 1, 3000);

    for (const VolumeInfo &volume : service.volumes()) {
        QVERIFY(!volume.canUnmount);
        QVERIFY(!volume.ejectable);
    }
}

void MountsTest::disconnectedVolumeRetainedAndReconnected()
{
    // §3.3/§17.8: a known volume that disappears stays in the model as
    // disconnected (amber badge) until the grace period elapses, and a
    // reconnect restores it as connected.
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString fixturePath = QDir(dir.path()).filePath(QStringLiteral("mountinfo"));

    QFile file(fixturePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(kMountInfo);
    file.close();

    MountService service;
    service.setMountInfoPath(fixturePath);
    service.setEnrichmentPaths(emptyEnrichmentPaths(dir.path()));
    service.setDisconnectedGraceCycles(5);

    QSignalSpy changed(&service, &MountService::volumesChanged);
    service.refresh();
    QTRY_COMPARE_WITH_TIMEOUT(changed.count(), 1, 3000);
    QCOMPARE(service.volumes().size(), 3);

    // Disconnect: the volume is retained, marked disconnected and unmounted.
    file.open(QIODevice::WriteOnly | QIODevice::Truncate);
    file.write(kMountInfoNoUsb);
    file.close();
    changed.clear();
    service.refresh();
    QTRY_COMPARE_WITH_TIMEOUT(changed.count(), 1, 3000);

    const QList<VolumeInfo> afterDisconnect = service.volumes();
    QCOMPARE(afterDisconnect.size(), 3);
    bool foundDisconnected = false;
    for (const VolumeInfo &volume : afterDisconnect) {
        if (volume.mountPoint == QStringLiteral("/media/usb")) {
            foundDisconnected = true;
            QVERIFY(!volume.connected);
            QVERIFY(!volume.mounted);
            QVERIFY(!volume.canUnmount);
            QVERIFY(!volume.ejectable);
        }
    }
    QVERIFY(foundDisconnected);

    // Reconnect: the retained placeholder is replaced by the real mount.
    file.open(QIODevice::WriteOnly | QIODevice::Truncate);
    file.write(kMountInfo);
    file.close();
    changed.clear();
    service.refresh();
    QTRY_COMPARE_WITH_TIMEOUT(changed.count(), 1, 3000);

    const QList<VolumeInfo> afterReconnect = service.volumes();
    QCOMPARE(afterReconnect.size(), 3);
    for (const VolumeInfo &volume : afterReconnect) {
        if (volume.mountPoint == QStringLiteral("/media/usb")) {
            QVERIFY(volume.connected);
            QVERIFY(volume.mounted);
        }
    }
}

void MountsTest::disconnectedVolumeAgesOut()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString fixturePath = QDir(dir.path()).filePath(QStringLiteral("mountinfo"));

    QFile file(fixturePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(kMountInfo);
    file.close();

    MountService service;
    service.setMountInfoPath(fixturePath);
    service.setEnrichmentPaths(emptyEnrichmentPaths(dir.path()));
    service.setDisconnectedGraceCycles(1); // one cycle of grace

    QSignalSpy changed(&service, &MountService::volumesChanged);
    service.refresh();
    QTRY_COMPARE_WITH_TIMEOUT(changed.count(), 1, 3000);

    file.open(QIODevice::WriteOnly | QIODevice::Truncate);
    file.write(kMountInfoNoUsb);
    file.close();

    // First cycle after the disconnect retains it, the next one drops it.
    changed.clear();
    service.refresh();
    QTRY_COMPARE_WITH_TIMEOUT(changed.count(), 1, 3000);
    QCOMPARE(service.volumes().size(), 3);

    changed.clear();
    service.refresh();
    QTRY_COMPARE_WITH_TIMEOUT(changed.count(), 1, 3000);
    const QList<VolumeInfo> volumes = service.volumes();
    QCOMPARE(volumes.size(), 2);
    for (const VolumeInfo &volume : volumes) {
        QVERIFY(volume.mountPoint != QStringLiteral("/media/usb"));
    }
}

void MountsTest::systemMountsAreNeverActedOn()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString fixturePath = QDir(dir.path()).filePath(QStringLiteral("mountinfo"));
    QFile file(fixturePath);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(kClassifiedMountInfo);
    file.close();

    MountService service;
    service.setMountInfoPath(fixturePath);
    service.setEnrichmentPaths(emptyEnrichmentPaths(dir.path()));
    // Even with an ejector installed, the safety gate must refuse the system
    // volume before the ejector is ever called.
    bool ejectorCalled = false;
    service.setEjector([&ejectorCalled](const VolumeInfo &, QString *) {
        ejectorCalled = true;
        return true;
    });

    QSignalSpy changed(&service, &MountService::volumesChanged);
    service.refresh();
    QTRY_COMPARE_WITH_TIMEOUT(changed.count(), 1, 3000);

    QSignalSpy finished(&service, &MountService::ejectFinished);
    service.unmount(QStringLiteral("/"));
    QCOMPARE(finished.count(), 1);
    QVERIFY(!finished.takeFirst().at(1).toBool());
    QVERIFY(!ejectorCalled);

    // A system-managed mount point below the root is refused as well.
    service.unmount(QStringLiteral("/boot/efi"));
    QCOMPARE(finished.count(), 1);
    QVERIFY(!finished.takeFirst().at(1).toBool());
    QVERIFY(!ejectorCalled);
}

// Integration evidence from the *real* host: the production path (mountinfo +
// kernel enrichment + GVfs/UDisks2 enrichment) classifies this machine's mounts.
// Assertions are structural so the test stays portable; the classified table is
// logged for the work-item record (FIX-UI-003-B).
void MountsTest::realHostClassificationReport()
{
    if (!QFile::exists(QStringLiteral("/proc/self/mountinfo"))) {
        QSKIP("no /proc/self/mountinfo on this host");
    }

    MountService service;
    service.setGioEnrichmentEnabled(true);
    QSignalSpy changed(&service, &MountService::volumesChanged);
    service.refresh();
    QTRY_VERIFY_WITH_TIMEOUT(changed.count() >= 1, 20000);

    const QList<VolumeInfo> volumes = service.volumes();
    QVERIFY(!volumes.isEmpty());
    qInfo("classified %d mounts", int(volumes.size()));
    qInfo("  kind      | label                        | mount point                      | device         | rm | ej | um");

    bool sawRoot = false;
    for (const VolumeInfo &v : volumes) {
        QVERIFY(!v.mountPoint.isEmpty());
        QVERIFY(!v.label.isEmpty());

        if (v.device.startsWith(QLatin1String("/dev/loop"))) {
            QCOMPARE(v.kind, VolumeKind::Image); // image-backed, never user storage
            QVERIFY(!v.canUnmount);
            QVERIFY(!v.ejectable);
        }
        if (v.mountPoint == QLatin1String("/")) {
            sawRoot = true;
            QCOMPARE(v.kind, VolumeKind::System);
            QVERIFY(v.systemManaged);
            QVERIFY(!v.canUnmount); // QG never unmounts the root filesystem
        }
        qInfo("  %-9s | %-28s | %-32s | %-14s |  %d |  %d |  %d",
              qUtf8Printable(volumeKindName(v.kind)), qUtf8Printable(v.label),
              qUtf8Printable(v.mountPoint), qUtf8Printable(v.device),
              v.removable ? 1 : 0, v.ejectable ? 1 : 0, v.canUnmount ? 1 : 0);
    }
    QVERIFY(sawRoot);
}

QTEST_GUILESS_MAIN(MountsTest)
#include "test_mounts.moc"
