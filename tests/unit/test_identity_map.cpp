// Identity<->path mapping + reconciliation tests (Prompt 29).

#include <QtTest/QtTest>

#include <QDir>
#include <QTemporaryDir>

#include "core/types.hpp"
#include "persistence/database.hpp"
#include "persistence/repositories.hpp"

using namespace qg;

class IdentityMapTest : public QObject
{
    Q_OBJECT

private slots:
    void renamePreservesMediaState();
    void unknownExternalMoveAdoptsOnRescan();
    void vanishedIdentityRetainedWithinGrace();
};

void IdentityMapTest::renamePreservesMediaState()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Database db;
    QVERIFY(db.open(QDir(dir.path()).filePath(QStringLiteral("qg.db"))));

    PathRepository paths(db);
    MediaStateRepository media(db);

    const QString identity = QStringLiteral("inode:9:9");
    QVERIFY(paths.upsert(QStringLiteral("/a/x.mp4"), identity, 9, 9, 100, 200));
    QVERIFY(media.setFavorite(identity, true));

    // Known rename via QG: mapping moves, media_state (keyed by identity) stays.
    QVERIFY(paths.renamePath(QStringLiteral("/a/x.mp4"), QStringLiteral("/a/y.mp4"), identity));
    QCOMPARE(paths.identityForPath(QStringLiteral("/a/y.mp4")), identity);
    QVERIFY(paths.identityForPath(QStringLiteral("/a/x.mp4")).isEmpty());
    QVERIFY(media.favorite(identity));
}

void IdentityMapTest::unknownExternalMoveAdoptsOnRescan()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Database db;
    QVERIFY(db.open(QDir(dir.path()).filePath(QStringLiteral("qg.db"))));

    PathRepository paths(db);
    const QString identity = QStringLiteral("inode:7:7");
    QVERIFY(paths.upsert(QStringLiteral("/a/x.mp4"), identity, 7, 7, 100, 200));

    // The file was moved externally; the snapshot now sees it at a new path with
    // the same inode identity.
    Entry moved;
    moved.kind = EntryKind::Video;
    moved.path = QStringLiteral("/b/x.mp4");
    moved.identity = identity;
    moved.size = 100;
    moved.mtimeNs = 200;

    QVERIFY(paths.reconcileSnapshot({moved}));
    QCOMPARE(paths.identityForPath(QStringLiteral("/b/x.mp4")), identity);
    QCOMPARE(paths.pathForIdentity(identity), QStringLiteral("/b/x.mp4"));
}

void IdentityMapTest::vanishedIdentityRetainedWithinGrace()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Database db;
    QVERIFY(db.open(QDir(dir.path()).filePath(QStringLiteral("qg.db"))));

    PathRepository paths(db);
    const QString identity = QStringLiteral("inode:5:5");
    QVERIFY(paths.upsert(QStringLiteral("/gone/x.mp4"), identity, 5, 5, 100, 200));

    // A snapshot that no longer contains the file must not delete its mapping.
    QVERIFY(paths.reconcileSnapshot({}));
    QCOMPARE(paths.identityForPath(QStringLiteral("/gone/x.mp4")), identity);

    const Result<int> pruned = paths.pruneVanished(30LL * 24 * 3600);
    QVERIFY(pruned);
    QCOMPARE(pruned.value(), 0); // still within the grace period
    QCOMPARE(paths.identityForPath(QStringLiteral("/gone/x.mp4")), identity);
}

QTEST_GUILESS_MAIN(IdentityMapTest)
#include "test_identity_map.moc"
