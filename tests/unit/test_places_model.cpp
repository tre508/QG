// PlacesModel tests (Prompt 11; semantic roles + section rules added by FIX-UI-003).

#include <QtTest/QtTest>

#include <QByteArray>
#include <QSignalSpy>
#include <QStandardPaths>

#include "models/places_model.hpp"

using namespace qg;

namespace {

VolumeInfo makeVolume(const QString &mountPoint,
                      const QString &label,
                      VolumeKind kind,
                      const QString &device = QString())
{
    VolumeInfo volume;
    volume.id = mountPoint;
    volume.mountPoint = mountPoint;
    volume.label = label;
    volume.device = device;
    volume.kind = kind;
    volume.systemManaged = kind == VolumeKind::System;
    volume.network = kind == VolumeKind::Network;
    volume.removable = kind == VolumeKind::Removable || kind == VolumeKind::Optical;
    volume.ejectable = kind == VolumeKind::Optical;
    volume.canUnmount = kind == VolumeKind::Removable || kind == VolumeKind::Network;
    return volume;
}

QVariant roleAt(const PlacesModel &model, int row, int role)
{
    return model.data(model.index(row, 0), role);
}

bool isHeaderRow(const PlacesModel &model, int row)
{
    return roleAt(model, row, PlacesModel::HeaderRole).toBool();
}

int rowForPath(const PlacesModel &model, const QString &path)
{
    for (int row = 0; row < model.rowCount(); ++row) {
        if (roleAt(model, row, PlacesModel::PathRole).toString() == path) {
            return row;
        }
    }
    return -1;
}

} // namespace

class PlacesModelTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void xdgPlacesPopulatePlacesGroup();
    void volumesPopulateDevicesGroup();
    void pinnedAndRecentGroupsPopulate();
    void groupsAppearInOrder();

    void semanticRolesResolveIconsLabelsAndTooltips();
    void groupHeadingsAreRowsAndEmptyGroupsAreOmitted();
    void imageAndVirtualVolumesAreNotSidebarDevices();
    void devicesAreOrderedDeterministically();
    void disconnectedVolumeKeepsIdentityAndShowsStatus();
    void capabilityRolesMirrorVolumeInfo();
    void repeatedSetVolumesDoesNotResetTheModel();
    void pinRequestsAreForwarded();
};

void PlacesModelTest::initTestCase()
{
    // Keep the XDG locations deterministic and independent of the real home.
    qputenv("HOME", QByteArrayLiteral("/tmp/qg-places-test-home"));
}

void PlacesModelTest::xdgPlacesPopulatePlacesGroup()
{
    PlacesModel model;
    QCOMPARE(model.countInGroup(PlacesModel::Places), 5);

    bool foundHome = false;
    for (int row = 0; row < model.rowCount(); ++row) {
        const QModelIndex index = model.index(row, 0);
        if (model.data(index, PlacesModel::LabelRole).toString() == QStringLiteral("Home")) {
            foundHome = true;
            QCOMPARE(model.data(index, PlacesModel::GroupRole).toInt(),
                     static_cast<int>(PlacesModel::Places));
        }
    }
    QVERIFY(foundHome);
}

void PlacesModelTest::volumesPopulateDevicesGroup()
{
    PlacesModel model;

    VolumeInfo usb;
    usb.id = QStringLiteral("/media/usb");
    usb.mountPoint = QStringLiteral("/media/usb");
    usb.label = QStringLiteral("USB Stick");
    usb.removable = true;
    usb.ejectable = true;

    VolumeInfo disk;
    disk.id = QStringLiteral("/mnt/disk");
    disk.mountPoint = QStringLiteral("/mnt/disk");
    disk.label = QStringLiteral("Data");

    model.setVolumes({usb, disk});
    QCOMPARE(model.countInGroup(PlacesModel::Devices), 2);

    bool checkedRemovable = false;
    for (int row = 0; row < model.rowCount(); ++row) {
        const QModelIndex index = model.index(row, 0);
        if (model.data(index, PlacesModel::PathRole).toString() == QStringLiteral("/media/usb")) {
            QCOMPARE(model.data(index, PlacesModel::RemovableRole).toBool(), true);
            QCOMPARE(model.data(index, PlacesModel::EjectableRole).toBool(), true);
            QCOMPARE(model.data(index, PlacesModel::LabelRole).toString(), QStringLiteral("USB Stick"));
            checkedRemovable = true;
        }
    }
    QVERIFY(checkedRemovable);
}

void PlacesModelTest::pinnedAndRecentGroupsPopulate()
{
    PlacesModel model;
    model.setPinned({QStringLiteral("/home/u/Projects")});
    model.setRecent({QStringLiteral("/home/u/Videos"), QStringLiteral("/home/u/Downloads")});

    QCOMPARE(model.countInGroup(PlacesModel::Pinned), 1);
    QCOMPARE(model.countInGroup(PlacesModel::Recent), 2);
}

void PlacesModelTest::groupsAppearInOrder()
{
    PlacesModel model;
    model.setPinned({QStringLiteral("/p")});
    model.setVolumes({});
    model.setRecent({QStringLiteral("/r")});

    int previous = -1;
    for (int row = 0; row < model.rowCount(); ++row) {
        const int group = model.data(model.index(row, 0), PlacesModel::GroupRole).toInt();
        QVERIFY(group >= previous);
        previous = group;
    }
}

void PlacesModelTest::semanticRolesResolveIconsLabelsAndTooltips()
{
    // Every row must carry the icon name of a shipped monoline glyph and a
    // human-readable label, so the delegate contains no heuristics (VIS-005).
    PlacesModel model;
    model.setPinned({QStringLiteral("/home/u/Projects")});
    model.setRecent({QStringLiteral("/home/u/Videos")});
    model.setVolumes({makeVolume(QStringLiteral("/mnt/data"), QStringLiteral("Data"), VolumeKind::Internal),
                      makeVolume(QStringLiteral("/media/usb"), QStringLiteral("USB Stick"),
                                 VolumeKind::Removable, QStringLiteral("/dev/sdb1")),
                      makeVolume(QStringLiteral("/mnt/nas"), QStringLiteral("NAS"), VolumeKind::Network),
                      makeVolume(QStringLiteral("/"), QStringLiteral(""), VolumeKind::System)});

    const QStringList allowedIcons{"home",   "videos", "downloads", "desktop", "documents",
                                   "recent", "pin",    "drive",     "drive-removable", "network"};

    int locationRows = 0;
    for (int row = 0; row < model.rowCount(); ++row) {
        if (isHeaderRow(model, row)) {
            continue;
        }
        ++locationRows;
        const QString icon = roleAt(model, row, PlacesModel::IconRole).toString();
        const QString kind = roleAt(model, row, PlacesModel::KindRole).toString();
        QVERIFY2(allowedIcons.contains(icon),
                 qPrintable(QStringLiteral("unexpected icon '%1' for %2").arg(icon, kind)));
        QVERIFY(!kind.isEmpty());
        QVERIFY(!roleAt(model, row, PlacesModel::LabelRole).toString().isEmpty());
        QVERIFY(!roleAt(model, row, PlacesModel::TooltipRole).toString().isEmpty());
    }
    QVERIFY(locationRows >= 5);

    // The root volume has no basename; it still needs a readable label.
    const int rootRow = rowForPath(model, QStringLiteral("/"));
    QVERIFY(rootRow >= 0);
    QCOMPARE(roleAt(model, rootRow, PlacesModel::LabelRole).toString(),
             QStringLiteral("Root Filesystem"));
    QCOMPARE(roleAt(model, rootRow, PlacesModel::IconRole).toString(), QStringLiteral("drive"));

    // Place rows map to their own glyphs.
    const int homeRow = rowForPath(model, QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
    QVERIFY(homeRow >= 0);
    QCOMPARE(roleAt(model, homeRow, PlacesModel::IconRole).toString(), QStringLiteral("home"));
    QCOMPARE(roleAt(model, homeRow, PlacesModel::KindRole).toString(), QStringLiteral("place"));

    const int recentRow = rowForPath(model, QStringLiteral("/home/u/Videos"));
    QVERIFY(recentRow >= 0);
    QCOMPARE(roleAt(model, recentRow, PlacesModel::IconRole).toString(), QStringLiteral("recent"));
    QCOMPARE(roleAt(model, recentRow, PlacesModel::KindRole).toString(), QStringLiteral("recent"));
}

void PlacesModelTest::groupHeadingsAreRowsAndEmptyGroupsAreOmitted()
{
    PlacesModel model;
    // Empty Pinned / Devices / Recent: only the PLACES group exists.
    model.setPinned({});
    model.setRecent({});
    model.setVolumes({});

    QCOMPARE(model.countInGroup(PlacesModel::Pinned), 0);
    QCOMPARE(model.countInGroup(PlacesModel::Devices), 0);
    QCOMPARE(model.countInGroup(PlacesModel::Recent), 0);

    QVERIFY(model.isHeaderAt(0));
    QCOMPARE(roleAt(model, 0, PlacesModel::LabelRole).toString(), QStringLiteral("PLACES"));
    QCOMPARE(roleAt(model, 0, PlacesModel::KindRole).toString(), QStringLiteral("header"));
    QCOMPARE(roleAt(model, 0, PlacesModel::IconRole).toString(), QString());
    QVERIFY(roleAt(model, 0, PlacesModel::PathRole).toString().isEmpty());

    QStringList headings;
    for (int row = 0; row < model.rowCount(); ++row) {
        if (isHeaderRow(model, row)) {
            headings << roleAt(model, row, PlacesModel::LabelRole).toString();
        }
    }
    QCOMPARE(headings, QStringList({QStringLiteral("PLACES")}));

    // Every non-empty group then contributes exactly one heading, in order.
    model.setPinned({QStringLiteral("/home/u/Projects")});
    model.setRecent({QStringLiteral("/home/u/Videos")});
    model.setVolumes({makeVolume(QStringLiteral("/mnt/data"), QStringLiteral("Data"),
                                 VolumeKind::Internal)});

    headings.clear();
    int previousGroup = -1;
    for (int row = 0; row < model.rowCount(); ++row) {
        const int group = roleAt(model, row, PlacesModel::GroupRole).toInt();
        QVERIFY(group >= previousGroup); // deterministic group order
        previousGroup = group;
        if (isHeaderRow(model, row)) {
            headings << roleAt(model, row, PlacesModel::LabelRole).toString();
            // A heading is the first row of its group.
            if (row > 0) {
                QVERIFY(roleAt(model, row - 1, PlacesModel::GroupRole).toInt() != group);
            }
        }
    }
    QCOMPARE(headings, QStringList({QStringLiteral("PINNED"), QStringLiteral("PLACES"),
                                    QStringLiteral("DEVICES"), QStringLiteral("RECENT")}));

    // Headings are not counted as locations.
    QCOMPARE(model.countInGroup(PlacesModel::Pinned), 1);
    QCOMPARE(model.countInGroup(PlacesModel::Devices), 1);
    QCOMPARE(model.countInGroup(PlacesModel::Recent), 1);
}

void PlacesModelTest::imageAndVirtualVolumesAreNotSidebarDevices()
{
    // Loopback image mounts and local desktop plumbing are classified but are
    // not user-browsable devices; everything else stays discoverable.
    PlacesModel model;
    model.setVolumes({makeVolume(QStringLiteral("/snap/core22/2437"), QStringLiteral("2437"),
                                 VolumeKind::Image, QStringLiteral("/dev/loop4")),
                      makeVolume(QStringLiteral("/run/user/1000/doc"), QStringLiteral("doc"),
                                 VolumeKind::Virtual),
                      makeVolume(QStringLiteral("/mnt/data"), QStringLiteral("Data"),
                                 VolumeKind::Internal),
                      makeVolume(QStringLiteral("/"), QStringLiteral(""), VolumeKind::System)});

    QCOMPARE(model.countInGroup(PlacesModel::Devices), 2);
    QVERIFY(rowForPath(model, QStringLiteral("/snap/core22/2437")) < 0);
    QVERIFY(rowForPath(model, QStringLiteral("/run/user/1000/doc")) < 0);
    QVERIFY(rowForPath(model, QStringLiteral("/mnt/data")) >= 0);
    QVERIFY(rowForPath(model, QStringLiteral("/")) >= 0);
}

void PlacesModelTest::devicesAreOrderedDeterministically()
{
    PlacesModel model;
    model.setVolumes({makeVolume(QStringLiteral("/boot/efi"), QStringLiteral("EFI"), VolumeKind::System),
                      makeVolume(QStringLiteral("/mnt/nas"), QStringLiteral("NAS"), VolumeKind::Network),
                      makeVolume(QStringLiteral("/media/usb"), QStringLiteral("USB Stick"),
                                 VolumeKind::Removable, QStringLiteral("/dev/sdb1")),
                      makeVolume(QStringLiteral("/mnt/data"), QStringLiteral("Data"), VolumeKind::Internal),
                      makeVolume(QStringLiteral("/media/cd"), QStringLiteral("DVD"), VolumeKind::Optical)});

    QStringList kinds;
    for (int row = 0; row < model.rowCount(); ++row) {
        if (roleAt(model, row, PlacesModel::GroupRole).toInt() == PlacesModel::Devices
            && !isHeaderRow(model, row)) {
            kinds << roleAt(model, row, PlacesModel::KindRole).toString();
        }
    }
    QCOMPARE(kinds, QStringList({"internal", "removable", "optical", "network", "system"}));

    // Re-applying the identical set yields the identical order, with no model
    // reset (the reconciliation poll must not disturb the view).
    QSignalSpy reset(&model, &QAbstractItemModel::modelReset);
    model.setVolumes({makeVolume(QStringLiteral("/boot/efi"), QStringLiteral("EFI"), VolumeKind::System),
                      makeVolume(QStringLiteral("/mnt/nas"), QStringLiteral("NAS"), VolumeKind::Network),
                      makeVolume(QStringLiteral("/media/usb"), QStringLiteral("USB Stick"),
                                 VolumeKind::Removable, QStringLiteral("/dev/sdb1")),
                      makeVolume(QStringLiteral("/mnt/data"), QStringLiteral("Data"), VolumeKind::Internal),
                      makeVolume(QStringLiteral("/media/cd"), QStringLiteral("DVD"), VolumeKind::Optical)});
    QCOMPARE(reset.count(), 0);

    QStringList again;
    for (int row = 0; row < model.rowCount(); ++row) {
        if (roleAt(model, row, PlacesModel::GroupRole).toInt() == PlacesModel::Devices
            && !isHeaderRow(model, row)) {
            again << roleAt(model, row, PlacesModel::KindRole).toString();
        }
    }
    QCOMPARE(again, kinds);
}

void PlacesModelTest::disconnectedVolumeKeepsIdentityAndShowsStatus()
{
    PlacesModel model;
    VolumeInfo usb = makeVolume(QStringLiteral("/media/usb"), QStringLiteral("USB Stick"),
                                VolumeKind::Removable, QStringLiteral("/dev/sdb1"));
    usb.connected = false;
    usb.mounted = false;
    usb.ejectable = false;
    usb.canUnmount = false;
    model.setVolumes({usb});

    const int row = rowForPath(model, QStringLiteral("/media/usb"));
    QVERIFY(row >= 0);
    QCOMPARE(roleAt(model, row, PlacesModel::ConnectedRole).toBool(), false);
    QCOMPARE(roleAt(model, row, PlacesModel::MountedRole).toBool(), false);
    QCOMPARE(roleAt(model, row, PlacesModel::CanEjectRole).toBool(), false);
    QCOMPARE(roleAt(model, row, PlacesModel::CanUnmountRole).toBool(), false);
    // Identity and navigation target survive the disconnect (§17.8).
    QCOMPARE(roleAt(model, row, PlacesModel::VolumeIdRole).toString(), QStringLiteral("/media/usb"));
    QCOMPARE(roleAt(model, row, PlacesModel::PathRole).toString(), QStringLiteral("/media/usb"));
    QVERIFY(roleAt(model, row, PlacesModel::TooltipRole).toString().contains(QStringLiteral("not connected")));
}

void PlacesModelTest::capabilityRolesMirrorVolumeInfo()
{
    PlacesModel model;
    model.setVolumes({makeVolume(QStringLiteral("/media/usb"), QStringLiteral("USB Stick"),
                                 VolumeKind::Removable, QStringLiteral("/dev/sdb1"))});

    const int row = rowForPath(model, QStringLiteral("/media/usb"));
    QVERIFY(row >= 0);
    QCOMPARE(roleAt(model, row, PlacesModel::CanUnmountRole).toBool(), true);
    QCOMPARE(roleAt(model, row, PlacesModel::CanEjectRole).toBool(), false);
    QCOMPARE(roleAt(model, row, PlacesModel::RemovableRole).toBool(), true);
    QCOMPARE(roleAt(model, row, PlacesModel::EjectableRole).toBool(), false);
}

void PlacesModelTest::repeatedSetVolumesDoesNotResetTheModel()
{
    // The reconciliation poll re-applies the same volume list every few
    // seconds; that must not drop the sidebar's current item or selection.
    PlacesModel model;
    const QList<VolumeInfo> volumes{
        makeVolume(QStringLiteral("/mnt/data"), QStringLiteral("Data"), VolumeKind::Internal),
        makeVolume(QStringLiteral("/mnt/nas"), QStringLiteral("NAS"), VolumeKind::Network)};
    model.setVolumes(volumes);

    QSignalSpy reset(&model, &QAbstractItemModel::modelReset);
    for (int i = 0; i < 4; ++i) {
        model.setVolumes(volumes);
    }
    QCOMPARE(reset.count(), 0);

    // A real change is published.
    QList<VolumeInfo> changed = volumes;
    changed[0].label = QStringLiteral("Data Disk");
    model.setVolumes(changed);
    QCOMPARE(reset.count(), 1);
}

void PlacesModelTest::pinRequestsAreForwarded()
{
    PlacesModel model;
    QSignalSpy pin(&model, &PlacesModel::pinRequested);
    QSignalSpy unpin(&model, &PlacesModel::unpinRequested);

    model.requestPin(QStringLiteral("/home/u/Projects"));
    model.requestUnpin(QStringLiteral("/home/u/Projects"));

    QCOMPARE(pin.count(), 1);
    QCOMPARE(pin.takeFirst().at(0).toString(), QStringLiteral("/home/u/Projects"));
    QCOMPARE(unpin.count(), 1);
    QCOMPARE(unpin.takeFirst().at(0).toString(), QStringLiteral("/home/u/Projects"));

    // Empty paths are ignored rather than persisted as a bookmark.
    model.requestPin(QString());
    QCOMPARE(pin.count(), 0);
}

QTEST_GUILESS_MAIN(PlacesModelTest)
#include "test_places_model.moc"
