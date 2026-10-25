// FolderModel contract tests (Prompt 10).

#include <QtTest/QtTest>

#include <QSignalSpy>

#include "models/folder_model.hpp"

using namespace qg;

namespace {

Entry makeVideo(const QString &name, const QString &identity)
{
    Entry entry;
    entry.kind = EntryKind::Video;
    entry.name = name;
    entry.path = QStringLiteral("/v/") + name;
    entry.identity = identity;
    entry.size = 1024;
    entry.mtimeNs = 42;
    return entry;
}

Entry makeFolder(const QString &name, const QString &identity)
{
    Entry entry;
    entry.kind = EntryKind::Folder;
    entry.name = name;
    entry.path = QStringLiteral("/v/") + name;
    entry.identity = identity;
    return entry;
}

} // namespace

class FolderModelTest : public QObject
{
    Q_OBJECT

private slots:
    void exposesCardRoles();
    void snapshotPopulatesRowsAndRoles();
    void batchInsertEmitsMinimalSignal();
    void updateStateEmitsTargetedDataChanged();
    void removeByIdentityEmitsRowsRemoved();
    void snapshotResetReplacesContent();
    void projectionFiltersVideosInOrder();
    void rowAccessorsExposeNameFolderAndSize();
    void identityAtReturnsCanonicalIdentity();
    void updatePathAndNameEmitsTargetedDataChanged();
};

void FolderModelTest::exposesCardRoles()
{
    FolderModel model;
    const QHash<int, QByteArray> roles = model.roleNames();
    for (const char *name : {"identity", "path", "name", "kind", "isFolder", "isVideo",
                             "thumbnail", "thumbnailState", "duration", "size", "mtime",
                             "favorite", "watched", "progress", "width", "height"}) {
        QVERIFY2(roles.values().contains(QByteArray(name)), name);
    }
}

void FolderModelTest::snapshotPopulatesRowsAndRoles()
{
    FolderModel model;
    FolderSnapshot snapshot;
    snapshot.entries = {makeFolder(QStringLiteral("sub"), QStringLiteral("inode:1:1")),
                        makeVideo(QStringLiteral("clip.mp4"), QStringLiteral("inode:1:2"))};
    model.applySnapshot(snapshot);

    QCOMPARE(model.rowCount(), 2);

    const QModelIndex folder = model.index(0, 0);
    QCOMPARE(model.data(folder, FolderModel::NameRole).toString(), QStringLiteral("sub"));
    QCOMPARE(model.data(folder, FolderModel::IsFolderRole).toBool(), true);
    QCOMPARE(model.data(folder, FolderModel::IsVideoRole).toBool(), false);
    QCOMPARE(model.data(folder, FolderModel::KindRole).toInt(),
             static_cast<int>(EntryKind::Folder));

    const QModelIndex video = model.index(1, 0);
    QCOMPARE(model.data(video, FolderModel::NameRole).toString(), QStringLiteral("clip.mp4"));
    QCOMPARE(model.data(video, FolderModel::SizeRole).toLongLong(), 1024);
    QCOMPARE(model.data(video, FolderModel::IsVideoRole).toBool(), true);
    // Unknown probe/thumbnail values are invalid, not zero.
    QVERIFY(!model.data(video, FolderModel::DurationRole).isValid());
    QVERIFY(!model.data(video, FolderModel::ThumbnailRole).isValid());
}

void FolderModelTest::batchInsertEmitsMinimalSignal()
{
    FolderModel model;
    QSignalSpy inserted(&model, &QAbstractItemModel::rowsInserted);

    FolderSnapshot batch;
    batch.entries = {makeFolder(QStringLiteral("a"), QStringLiteral("i1")),
                     makeVideo(QStringLiteral("b.mp4"), QStringLiteral("i2"))};
    model.applyBatch(batch);

    QCOMPARE(inserted.count(), 1);
    const QList<QVariant> args = inserted.takeFirst();
    QCOMPARE(args.at(1).toInt(), 0); // first
    QCOMPARE(args.at(2).toInt(), 1); // last
    QCOMPARE(model.rowCount(), 2);

    // A second batch appends without touching existing rows.
    FolderSnapshot second;
    second.entries = {makeVideo(QStringLiteral("c.mp4"), QStringLiteral("i3"))};
    model.applyBatch(second);
    QCOMPARE(model.rowCount(), 3);
    QCOMPARE(model.data(model.index(0, 0), FolderModel::NameRole).toString(), QStringLiteral("a"));
    QCOMPARE(model.data(model.index(2, 0), FolderModel::NameRole).toString(), QStringLiteral("c.mp4"));
}

void FolderModelTest::updateStateEmitsTargetedDataChanged()
{
    FolderModel model;
    FolderSnapshot snapshot;
    snapshot.entries = {makeVideo(QStringLiteral("clip.mp4"), QStringLiteral("k"))};
    model.applySnapshot(snapshot);

    QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
    model.updateState(QStringLiteral("k"), true, std::nullopt, 0.5);

    QCOMPARE(changed.count(), 1);
    const QList<QVariant> args = changed.takeFirst();
    QCOMPARE(args.at(0).toModelIndex().row(), 0);
    const QList<int> roles = args.at(2).value<QList<int>>();
    QVERIFY(roles.contains(FolderModel::FavoriteRole));
    QVERIFY(roles.contains(FolderModel::WatchedRole));
    QVERIFY(roles.contains(FolderModel::ProgressRole));

    QCOMPARE(model.data(model.index(0, 0), FolderModel::FavoriteRole).toBool(), true);
    QCOMPARE(model.data(model.index(0, 0), FolderModel::ProgressRole).toDouble(), 0.5);
}

void FolderModelTest::removeByIdentityEmitsRowsRemoved()
{
    FolderModel model;
    FolderSnapshot snapshot;
    snapshot.entries = {makeVideo(QStringLiteral("a.mp4"), QStringLiteral("a")),
                        makeVideo(QStringLiteral("b.mp4"), QStringLiteral("b"))};
    model.applySnapshot(snapshot);

    QSignalSpy removed(&model, &QAbstractItemModel::rowsRemoved);
    QVERIFY(model.removeByIdentity(QStringLiteral("a")));

    QCOMPARE(removed.count(), 1);
    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(model.data(model.index(0, 0), FolderModel::IdentityRole).toString(),
             QStringLiteral("b"));

    QVERIFY(!model.removeByIdentity(QStringLiteral("missing")));
}

void FolderModelTest::snapshotResetReplacesContent()
{
    FolderModel model;
    FolderSnapshot first;
    first.entries = {makeVideo(QStringLiteral("a.mp4"), QStringLiteral("a"))};
    model.applySnapshot(first);

    FolderSnapshot second;
    second.entries = {makeFolder(QStringLiteral("x"), QStringLiteral("x")),
                      makeFolder(QStringLiteral("y"), QStringLiteral("y"))};
    model.applySnapshot(second);

    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.data(model.index(0, 0), FolderModel::IsFolderRole).toBool(), true);
}

void FolderModelTest::projectionFiltersVideosInOrder()
{
    FolderModel model;
    FolderSnapshot snapshot;
    snapshot.entries = {makeVideo(QStringLiteral("a.mp4"), QStringLiteral("a")),
                        makeFolder(QStringLiteral("sub"), QStringLiteral("s")),
                        makeVideo(QStringLiteral("b.mp4"), QStringLiteral("b"))};
    model.applySnapshot(snapshot);

    const QList<Entry> videos =
        model.entriesMatching([](const Entry &entry) { return entry.isVideo(); });
    QCOMPARE(videos.size(), 2);
    QCOMPARE(videos.at(0).name, QStringLiteral("a.mp4"));
    QCOMPARE(videos.at(1).name, QStringLiteral("b.mp4"));
}

void FolderModelTest::rowAccessorsExposeNameFolderAndSize()
{
    FolderModel model;
    FolderSnapshot snapshot;
    snapshot.entries = {makeFolder(QStringLiteral("sub"), QStringLiteral("s")),
                        makeVideo(QStringLiteral("clip.mp4"), QStringLiteral("v"))};
    model.applySnapshot(snapshot);

    // QML-facing accessors used by the status strip's selection summary.
    QCOMPARE(model.sizeAt(1), 1024);
    QCOMPARE(model.sizeAt(0), 0);          // folder size is 0 (unknown), not fabricated
    QCOMPARE(model.sizeAt(-1), 0);         // out-of-range is safe
    QCOMPARE(model.sizeAt(99), 0);
    QCOMPARE(model.nameAt(1), QStringLiteral("clip.mp4"));
    QCOMPARE(model.isFolderAt(0), true);
    QCOMPARE(model.isFolderAt(1), false);
}

void FolderModelTest::identityAtReturnsCanonicalIdentity()
{
    FolderModel model;
    FolderSnapshot snapshot;
    snapshot.entries = {makeFolder(QStringLiteral("sub"), QStringLiteral("inode:5:101")),
                        makeVideo(QStringLiteral("clip.mp4"), QStringLiteral("inode:5:202"))};
    model.applySnapshot(snapshot);

    // Canonical identity string is returned verbatim, not derived from path.
    QCOMPARE(model.identityAt(0), QStringLiteral("inode:5:101")); // folder
    QCOMPARE(model.identityAt(1), QStringLiteral("inode:5:202")); // video

    // Consistency with the identity-keyed model role.
    QCOMPARE(model.identityAt(0),
             model.data(model.index(0, 0), FolderModel::IdentityRole).toString());
    QCOMPARE(model.identityAt(1),
             model.data(model.index(1, 0), FolderModel::IdentityRole).toString());

    // Out-of-range is safe (matches the other row-accessor contract).
    QCOMPARE(model.identityAt(-1), QString());
    QCOMPARE(model.identityAt(99), QString());

    // Stable after re-sort: identity follows the row, not the path string.
    SortSpec spec;
    spec.key = SortKey::Name;
    spec.direction = SortDirection::Ascending;
    model.reorder(spec);
    const QStringList ids{model.identityAt(0), model.identityAt(1)};
    QCOMPARE(ids.size(), 2);
    QVERIFY(ids.contains(QStringLiteral("inode:5:101")));
    QVERIFY(ids.contains(QStringLiteral("inode:5:202")));
    QCOMPARE(ids[0].isEmpty(), false);

    // Q_INVOKABLE: QML `folderModel.identityAt(row)` must resolve (D-05).
    QVERIFY(model.metaObject()->indexOfMethod("identityAt(int)") >= 0);
}

void FolderModelTest::updatePathAndNameEmitsTargetedDataChanged()
{
    FolderModel model;
    FolderSnapshot snapshot;
    snapshot.entries = {makeVideo(QStringLiteral("old.mp4"), QStringLiteral("k"))};
    model.applySnapshot(snapshot);

    QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
    model.updatePathAndName(QStringLiteral("k"),
                            QStringLiteral("/v/new.mp4"),
                            QStringLiteral("new.mp4"));

    QCOMPARE(changed.count(), 1);
    const QList<QVariant> args = changed.takeFirst();
    QCOMPARE(args.at(0).toModelIndex().row(), 0);
    const QList<int> roles = args.at(2).value<QList<int>>();
    QVERIFY(roles.contains(FolderModel::PathRole));
    QVERIFY(roles.contains(FolderModel::NameRole));
    // Other roles should not be included
    QVERIFY(!roles.contains(FolderModel::FavoriteRole));
    QVERIFY(!roles.contains(FolderModel::WatchedRole));

    // Verify the data was updated
    QCOMPARE(model.data(model.index(0, 0), FolderModel::PathRole).toString(), QStringLiteral("/v/new.mp4"));
    QCOMPARE(model.data(model.index(0, 0), FolderModel::NameRole).toString(), QStringLiteral("new.mp4"));
    // Identity should remain unchanged
    QCOMPARE(model.data(model.index(0, 0), FolderModel::IdentityRole).toString(), QStringLiteral("k"));

    // Unknown identity is a no-op (no signal)
    changed.clear();
    model.updatePathAndName(QStringLiteral("missing"),
                            QStringLiteral("/v/x.mp4"),
                            QStringLiteral("x.mp4"));
    QCOMPARE(changed.count(), 0);
}

QTEST_GUILESS_MAIN(FolderModelTest)
#include "test_folder_model.moc"
