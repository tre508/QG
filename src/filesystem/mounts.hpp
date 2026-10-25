#pragma once

// Mount discovery (DESIGN.md §3.3, §17.4, §23.13–§23.14).
//
// `MountService` parses `/proc/self/mountinfo` (the kernel is authoritative),
// classifies each mount by *identity* (device source, filesystem type, mount
// point) rather than by path appearance, and reports volumes to `PlacesModel`.
// Removable/eject capability is enriched from authoritative platform sources —
// the kernel (`/sys/class/block/<disk>/removable`, `/dev/disk/by-label`) and
// the desktop volume monitor (GVfs/UDisks2 through `gio`, §17.4) — degrading
// gracefully to kernel mount data where desktop services are absent.
//
// Parsing, enrichment and the reconciliation poll run off the GUI thread;
// results are delivered queued.

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QMetaType>
#include <QObject>
#include <QString>

#include <functional>

class QTimer;

namespace qg {

// Semantic mount identity (DESIGN.md §3.3 "Devices"). Derived from kernel facts
// (device source, filesystem type, mount-point role) and, where available, from
// GVfs/UDisks2 capability data — never from the mount path alone.
enum class VolumeKind {
    System,    // system-managed mount of the boot/root device (/, /boot, /boot/efi, /usr, /var, /home, …)
    Internal,  // fixed, non-removable storage volume (data disk, extra partition)
    Removable, // removable media / USB-attached drive (authoritative: sysfs or UDisks2)
    Optical,   // optical media (/dev/sr*); ejectable by nature
    Network,   // remote filesystem (NFS/CIFS/SMB/SSH/WebDAV/GVfs share)
    Image,     // loopback image mount (snap squashfs, ISO loop) — not user storage
    Virtual,   // local desktop plumbing (portal documents, gvfsd-fuse, nsfs, zram)
};

// Stable lowercase key for the classification, exposed to QML as a role so no
// delegate re-derives the kind.
QString volumeKindName(VolumeKind kind);

struct VolumeInfo {
    QString id;         // stable identifier (mount point)
    QString mountPoint;
    QString device;     // kernel source (e.g. /dev/sdb1), may be "none"
    QString fsType;
    QString label;      // human-readable display label
    VolumeKind kind = VolumeKind::Internal;
    bool removable = false;     // authoritative: sysfs removable or UDisks2 drive is_removable
    bool ejectable = false;     // authoritative: UDisks2 can_eject, or optical media
    bool connected = true;      // false while a known volume is disconnected (grace retention)
    bool network = false;
    bool mounted = true;        // false for a retained (disconnected) volume
    bool canUnmount = false;    // authoritative: desktop reports unmount support (§17.4)
    bool systemManaged = false; // part of the running system; QG never unmounts it
};

// True for kernel/desktop pseudo-filesystems that are not user volumes.
bool isPseudoFilesystem(const QString &fsType);

// Pure parser for `/proc/self/mountinfo` content (testable with a fixture).
// Classification is kernel-only; no host state is consulted.
QList<VolumeInfo> parseMountInfo(const QByteArray &content);

// --- Kernel-state enrichment (runs off the GUI thread) ----------------------
// Roots of the kernel interfaces consulted during enrichment; overridable so
// tests drive a fixture instead of host state.
struct DiskEnrichmentPaths {
    QString byLabelDir = QStringLiteral("/dev/disk/by-label");
    QString sysBlockDir = QStringLiteral("/sys/class/block");
};

// Applies authoritative kernel facts `parseMountInfo` cannot know:
//   * `/dev/disk/by-label/*` reverse lookup → real filesystem labels,
//   * `/sys/class/block/<disk>/removable` → physical removability of the disk
//     backing the volume (never inferred from the mount path).
// Reads files only; safe on a worker thread; never throws.
void enrichVolumesFromDisk(QList<VolumeInfo> &volumes, const DiskEnrichmentPaths &paths = {});

// --- GVfs/UDisks2 capability enrichment (DESIGN.md §17.4) -------------------
// One desktop volume as reported by the GVfs/UDisks2 volume monitor.
struct GioMountEntry {
    QString device;       // unix-device, e.g. /dev/sdb1 (empty for remote shares)
    QString driveDevice;  // unix-device of the backing drive, e.g. /dev/sdb
    QString label;
    QString mountPath;    // local path when local (from default_location)
    QString uri;          // mount URI (may be remote, e.g. smb://host/share)
    bool driveRemovable = false;
    bool driveCanEject = false;
    bool canEject = false;
    bool canUnmount = false;
    bool network = false;
    bool mounted = false;
};

// Pure parser for `gio mount -l -i` output (GVfs/UDisks2-backed). Entries are
// keyed by unix device and by mount path. Testable with a captured fixture;
// performs no I/O.
QHash<QString, GioMountEntry> parseGioMountList(const QByteArray &content);

// Applies desktop-reported capabilities onto kernel-derived volumes (pure, no
// I/O). Matching is by device source first, then by mount path; volumes no
// desktop service knows about keep their kernel-derived classification.
void applyGioMountList(QList<VolumeInfo> &volumes, const QHash<QString, GioMountEntry> &entries);

// True when the `gio` desktop tool is on PATH (checked once, cached, non-blocking).
bool gioMountToolAvailable();

class MountService : public QObject {
    Q_OBJECT

public:
    explicit MountService(QObject *parent = nullptr);

    QList<VolumeInfo> volumes() const { return m_volumes; }

    // Begin/stop the low-frequency reconciliation poll.
    void start(int pollIntervalMs = 5000);
    void stop();

    // Optional enrichment hook (extra labels/ejectability). Runs off the GUI
    // thread, after the kernel and GVfs/UDisks2 enrichment steps.
    using Enricher = std::function<void(QList<VolumeInfo> &)>;
    void setEnricher(Enricher enricher) { m_enricher = std::move(enricher); }

    // Optional eject hook. When unset, eject/unmount run through `gio`
    // (GVfs/UDisks2) when it is available, otherwise they report unsupported.
    using Ejector = std::function<bool(const VolumeInfo &, QString *)>;
    void setEjector(Ejector ejector) { m_ejector = std::move(ejector); }

    // GVfs/UDisks2 (`gio mount -l -i`) capability enrichment. Off by default so
    // tests stay hermetic; the application enables it (DESIGN.md §17.4).
    void setGioEnrichmentEnabled(bool enabled) { m_gioEnrichment = enabled; }
    bool gioEnrichmentEnabled() const { return m_gioEnrichment; }

    // Test seam: override the mountinfo path. Empty means the real kernel path.
    void setMountInfoPath(const QString &path) { m_mountInfoPath = path; }
    void setEnrichmentPaths(const DiskEnrichmentPaths &paths) { m_enrichmentPaths = paths; }

    // How many reconciliation cycles a vanished user-relevant volume stays in
    // the model as disconnected (DESIGN.md §3.3 amber badge, §17.8 retention).
    void setDisconnectedGraceCycles(int cycles) { m_graceCycles = cycles; }
    int disconnectedGraceCycles() const { return m_graceCycles; }

public slots:
    void refresh();
    // Both report through ejectFinished(); both refuse system/critical mounts.
    void eject(const QString &volumeId);
    void unmount(const QString &volumeId);

signals:
    void volumesChanged(const QList<qg::VolumeInfo> &volumes);
    void ejectFinished(const QString &volumeId, bool ok, const QString &message);

private:
    void applyVolumes(QList<VolumeInfo> volumes);
    // Retains user-relevant volumes that disappeared so the sidebar can show a
    // disconnected state (§3.3) until the grace period elapses or they return.
    void reconcileDisconnected(QList<VolumeInfo> &volumes);
    bool runDeviceAction(const QString &volumeId, bool ejectRequested);

    QList<VolumeInfo> m_volumes;
    Enricher m_enricher;
    Ejector m_ejector;
    QTimer *m_pollTimer = nullptr;
    QString m_mountInfoPath;
    DiskEnrichmentPaths m_enrichmentPaths;
    bool m_gioEnrichment = false;
    int m_graceCycles = 60;

    struct Retained {
        VolumeInfo volume;
        int cyclesLeft = 0;
    };
    QHash<QString, Retained> m_retained;
};

} // namespace qg

Q_DECLARE_METATYPE(qg::VolumeInfo)
Q_DECLARE_METATYPE(QList<qg::VolumeInfo>)
