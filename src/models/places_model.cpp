#include "models/places_model.hpp"

#include <QCoreApplication>
#include <QFileInfo>
#include <QStandardPaths>

#include <algorithm>

namespace qg {
namespace {

struct XdgPlace {
    const char *label;
    QStandardPaths::StandardLocation location;
    const char *icon;
    const char *kind;
};

// Order per DESIGN.md §3.3. Icon names are monoline registry names (§2.3), not
// freedesktop theme names: the sidebar renders shipped SVGs through Icon.qml.
const XdgPlace kXdgPlaces[] = {
    {"Home", QStandardPaths::HomeLocation, "home", "place"},
    {"Videos", QStandardPaths::MoviesLocation, "videos", "place"},
    {"Downloads", QStandardPaths::DownloadLocation, "downloads", "place"},
    {"Desktop", QStandardPaths::DesktopLocation, "desktop", "place"},
    {"Documents", QStandardPaths::DocumentsLocation, "documents", "place"},
};

// Deterministic rank for the Devices group so the section reads predictably:
// user-facing storage first, system-managed mounts last.
int kindRank(const VolumeKind kind)
{
    switch (kind) {
    case VolumeKind::Internal:
        return 0;
    case VolumeKind::Removable:
        return 1;
    case VolumeKind::Optical:
        return 2;
    case VolumeKind::Network:
        return 3;
    case VolumeKind::System:
        return 4;
    case VolumeKind::Image:
        return 5;
    case VolumeKind::Virtual:
        return 6;
    }
    return 7;
}

// Semantic icon per verified mount identity (DESIGN.md §3.3, §4 wireframe).
QString iconForKind(const VolumeKind kind)
{
    switch (kind) {
    case VolumeKind::Removable:
    case VolumeKind::Optical:
        return QStringLiteral("drive-removable");
    case VolumeKind::Network:
        return QStringLiteral("network");
    case VolumeKind::System:
    case VolumeKind::Internal:
    case VolumeKind::Image:
    case VolumeKind::Virtual:
        break;
    }
    return QStringLiteral("drive");
}

// Human-readable device label. Never the raw mount point when something better
// is authoritative (volume label from udev/GVfs); "/" has no basename at all.
QString deviceLabel(const VolumeInfo &volume)
{
    if (!volume.label.isEmpty() && volume.label != volume.mountPoint) {
        return volume.label;
    }
    if (volume.mountPoint == QLatin1String("/")) {
        return QCoreApplication::translate("PlacesModel", "Root Filesystem");
    }
    const QString base = QFileInfo(volume.mountPoint).fileName();
    return base.isEmpty() ? volume.mountPoint : base;
}

QString deviceTooltip(const VolumeInfo &volume, const QString &label)
{
    if (!volume.connected) {
        return QCoreApplication::translate("PlacesModel", "%1 — not connected").arg(label);
    }
    if (volume.network) {
        return QCoreApplication::translate("PlacesModel", "%1 — network location (%2)")
            .arg(label, volume.mountPoint);
    }
    if (volume.systemManaged) {
        return QCoreApplication::translate("PlacesModel", "%1 — system volume (%2)")
            .arg(label, volume.mountPoint);
    }
    return volume.mountPoint;
}

} // namespace

PlacesModel::PlacesModel(QObject *parent)
    : QAbstractListModel(parent)
{
    rebuild();
}

int PlacesModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QVariant PlacesModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size()) {
        return {};
    }

    const Row &row = m_rows.at(index.row());
    switch (role) {
    case LabelRole:
        return row.label;
    case PathRole:
        return row.path;
    case IconRole:
        return row.icon;
    case GroupRole:
        return static_cast<int>(row.group);
    case ConnectedRole:
        return row.connected;
    case RemovableRole:
        return row.removable;
    case EjectableRole:
        return row.ejectable;
    case VolumeIdRole:
        return row.volumeId;
    case KindRole:
        return row.kind;
    case TooltipRole:
        return row.tooltip;
    case MountedRole:
        return row.mounted;
    case CanUnmountRole:
        return row.canUnmount;
    case CanEjectRole:
        return row.canEject;
    case HeaderRole:
        return row.header;
    default:
        return {};
    }
}

QHash<int, QByteArray> PlacesModel::roleNames() const
{
    return {
        {LabelRole, QByteArrayLiteral("label")},
        {PathRole, QByteArrayLiteral("path")},
        {IconRole, QByteArrayLiteral("icon")},
        {GroupRole, QByteArrayLiteral("group")},
        {ConnectedRole, QByteArrayLiteral("connected")},
        {RemovableRole, QByteArrayLiteral("removable")},
        {EjectableRole, QByteArrayLiteral("ejectable")},
        {VolumeIdRole, QByteArrayLiteral("volumeId")},
        {KindRole, QByteArrayLiteral("kind")},
        {TooltipRole, QByteArrayLiteral("tooltip")},
        {MountedRole, QByteArrayLiteral("mounted")},
        {CanUnmountRole, QByteArrayLiteral("canUnmount")},
        {CanEjectRole, QByteArrayLiteral("canEject")},
        {HeaderRole, QByteArrayLiteral("isHeader")},
    };
}

void PlacesModel::setPinned(const QStringList &paths)
{
    m_pinned = paths;
    rebuild();
}

void PlacesModel::setRecent(const QStringList &paths)
{
    m_recent = paths;
    rebuild();
}

void PlacesModel::setVolumes(const QList<VolumeInfo> &volumes)
{
    m_volumes = volumes;
    rebuild();
}

int PlacesModel::countInGroup(Group group) const
{
    int count = 0;
    for (const Row &row : m_rows) {
        if (row.group == group && !row.header) {
            ++count;
        }
    }
    return count;
}

bool PlacesModel::isHeaderAt(int row) const
{
    return row >= 0 && row < m_rows.size() && m_rows.at(row).header;
}

void PlacesModel::requestPin(const QString &path)
{
    if (!path.isEmpty()) {
        emit pinRequested(path);
    }
}

void PlacesModel::requestUnpin(const QString &path)
{
    if (!path.isEmpty()) {
        emit unpinRequested(path);
    }
}

void PlacesModel::rebuild()
{
    QList<Row> rows;

    // Group headings are ordinary rows of the same height as a location row.
    // That keeps one uniform delegate height, so the ListView's content height
    // is exact instead of estimated — a requirement for the scroll range (and
    // therefore the scrollbar) to cover every row. Empty groups emit no
    // heading.
    auto appendHeading = [&rows](Group group, const char *context, const char *text) {
        Row row;
        row.label = QCoreApplication::translate("PlacesModel", text);
        row.group = group;
        row.header = true;
        row.kind = QStringLiteral("header");
        row.tooltip = QString::fromUtf8(context);
        rows.append(row);
    };

    if (!m_pinned.isEmpty()) {
        appendHeading(Pinned, "pinned", "PINNED");
    }
    for (const QString &path : m_pinned) {
        Row row;
        row.label = QFileInfo(path).fileName();
        if (row.label.isEmpty()) {
            row.label = path;
        }
        row.path = path;
        row.icon = QStringLiteral("pin");
        row.kind = QStringLiteral("pinned");
        row.tooltip = path;
        row.group = Pinned;
        rows.append(row);
    }

    appendHeading(Places, "places", "PLACES");
    for (const XdgPlace &place : kXdgPlaces) {
        const QString path = QStandardPaths::writableLocation(place.location);
        Row row;
        row.label = QString::fromUtf8(place.label);
        row.path = path;
        row.icon = QString::fromUtf8(place.icon);
        row.kind = QString::fromUtf8(place.kind);
        row.tooltip = path;
        row.group = Places;
        rows.append(row);
    }

    // Devices: loopback image mounts (snap/ISO) and local desktop plumbing are
    // classified but deliberately not surfaced as sidebar devices — they are
    // not storage the user browses (DESIGN.md §3.3 "mounted volumes"; §17.4
    // filters pseudo-filesystems). Everything else stays discoverable.
    QList<VolumeInfo> devices;
    devices.reserve(m_volumes.size());
    for (const VolumeInfo &volume : m_volumes) {
        if (volume.kind == VolumeKind::Image || volume.kind == VolumeKind::Virtual) {
            continue;
        }
        devices.append(volume);
    }
    std::stable_sort(devices.begin(), devices.end(), [](const VolumeInfo &a, const VolumeInfo &b) {
        const int rankA = kindRank(a.kind);
        const int rankB = kindRank(b.kind);
        if (rankA != rankB) {
            return rankA < rankB;
        }
        const int byLabel = QString::compare(a.label, b.label, Qt::CaseInsensitive);
        if (byLabel != 0) {
            return byLabel < 0;
        }
        return a.mountPoint < b.mountPoint;
    });

    if (!devices.isEmpty()) {
        appendHeading(Devices, "devices", "DEVICES");
    }
    for (const VolumeInfo &volume : devices) {
        Row row;
        row.label = deviceLabel(volume);
        row.path = volume.mountPoint;
        row.icon = iconForKind(volume.kind);
        row.kind = volumeKindName(volume.kind);
        row.tooltip = deviceTooltip(volume, row.label);
        row.group = Devices;
        row.connected = volume.connected;
        row.removable = volume.removable;
        row.ejectable = volume.ejectable;
        row.volumeId = volume.id;
        row.mounted = volume.mounted;
        row.canUnmount = volume.canUnmount;
        row.canEject = volume.ejectable;
        rows.append(row);
    }

    if (!m_recent.isEmpty()) {
        appendHeading(Recent, "recent", "RECENT");
    }
    for (const QString &path : m_recent) {
        Row row;
        row.label = QFileInfo(path).fileName();
        if (row.label.isEmpty()) {
            row.label = path;
        }
        row.path = path;
        row.icon = QStringLiteral("recent");
        row.kind = QStringLiteral("recent");
        row.tooltip = path;
        row.group = Recent;
        rows.append(row);
    }

    // A periodic reconciliation poll re-applies the same volume list; resetting
    // the model would drop the sidebar's current item and any selection four
    // times a minute. Publish only real changes (§14.2 stable roles).
    auto same = [](const Row &a, const Row &b) {
        return a.label == b.label && a.path == b.path && a.icon == b.icon
               && a.volumeId == b.volumeId && a.kind == b.kind && a.tooltip == b.tooltip
               && a.group == b.group && a.connected == b.connected && a.removable == b.removable
               && a.ejectable == b.ejectable && a.mounted == b.mounted && a.header == b.header
               && a.canUnmount == b.canUnmount && a.canEject == b.canEject;
    };
    if (rows.size() == m_rows.size()) {
        bool identical = true;
        for (int i = 0; i < rows.size(); ++i) {
            if (!same(rows.at(i), m_rows.at(i))) {
                identical = false;
                break;
            }
        }
        if (identical) {
            return;
        }
    }

    beginResetModel();
    m_rows = std::move(rows);
    endResetModel();
}

} // namespace qg
