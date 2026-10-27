// SQLite persistence foundation tests (Prompt 06).
//
// Uses temporary databases only. Verifies WAL + schema, migration idempotency,
// reopen durability, corrupt-DB quarantine, a second connection reading while a
// write occurs, and the repository round-trips.

#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QTemporaryDir>

#include "persistence/database.hpp"
#include "persistence/repositories.hpp"

using namespace qg;

class PersistenceTest : public QObject
{
    Q_OBJECT

private slots:
    void opensInWalWithSchema();
    void migrationIsIdempotent();
    void reopenPreservesState();
    void corruptDatabaseIsQuarantinedNotDestroyed();
    void secondConnectionReadsAfterWrite();
    void repositoriesRoundTrip();
    void writeFailureIsReturnedWhenQueryOnly();

private:
    static QString dbPath(const QTemporaryDir &dir)
    {
        return QDir(dir.path()).filePath(QStringLiteral("qg.db"));
    }
};

void PersistenceTest::opensInWalWithSchema()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    Database db;
    QVERIFY(db.open(dbPath(dir)));
    QVERIFY(db.isOpen());
    QCOMPARE(db.schemaVersion(), 1);

    Statement journal(db, QStringLiteral("PRAGMA journal_mode;"));
    QVERIFY(journal.isValid());
    QVERIFY(journal.step());
    QCOMPARE(journal.columnText(0).toLower(), QStringLiteral("wal"));

    Statement fk(db, QStringLiteral("PRAGMA foreign_keys;"));
    QVERIFY(fk.step());
    QCOMPARE(fk.columnInt64(0), 1);
}

void PersistenceTest::migrationIsIdempotent()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dbPath(dir);

    {
        Database db;
        QVERIFY(db.open(path));
        QCOMPARE(db.schemaVersion(), 1);
    }
    {
        Database db;
        QVERIFY(db.open(path));
        QCOMPARE(db.schemaVersion(), 1);
    }
}

void PersistenceTest::reopenPreservesState()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dbPath(dir);
    const QString identity = QStringLiteral("inode:801:3039");

    {
        Database db;
        QVERIFY(db.open(path));
        MediaStateRepository media(db);
        QVERIFY(media.setFavorite(identity, true));
        QVERIFY(media.recordPosition(identity, 42000));
    }

    Database db;
    QVERIFY(db.open(path));
    MediaStateRepository media(db);
    QVERIFY(media.favorite(identity));
    QCOMPARE(media.positionMs(identity), 42000);
}

void PersistenceTest::corruptDatabaseIsQuarantinedNotDestroyed()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dbPath(dir);

    {
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QByteArray(4096, 'x'));
    }

    Database db;
    QVERIFY(db.open(path));
    QVERIFY(db.recoveredCorruptDatabase());
    QVERIFY(!db.quarantinedPath().isEmpty());
    QVERIFY(QFile::exists(db.quarantinedPath()));

    // Fresh database is valid.
    QCOMPARE(db.schemaVersion(), 1);
    Statement check(db, QStringLiteral("PRAGMA quick_check;"));
    QVERIFY(check.step());
    QCOMPARE(check.columnText(0), QStringLiteral("ok"));
}

void PersistenceTest::secondConnectionReadsAfterWrite()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dbPath(dir);
    const QString identity = QStringLiteral("inode:1:2");

    Database writer;
    Database reader;
    QVERIFY(writer.open(path));
    QVERIFY(reader.open(path));

    MediaStateRepository writeState(writer);
    MediaStateRepository readState(reader);

    QVERIFY(writeState.setWatched(identity, true));
    QVERIFY(readState.watched(identity));
}

void PersistenceTest::repositoriesRoundTrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Database db;
    QVERIFY(db.open(dbPath(dir)));

    // paths
    PathRepository paths(db);
    QVERIFY(paths.upsert(QStringLiteral("/v/a.mp4"),
                         QStringLiteral("inode:9:9"),
                         9,
                         9,
                         123,
                         456));
    QCOMPARE(paths.identityForPath(QStringLiteral("/v/a.mp4")), QStringLiteral("inode:9:9"));

    // media_state
    MediaStateRepository media(db);
    QVERIFY(media.setFavorite(QStringLiteral("k"), true));
    QVERIFY(media.setWatched(QStringLiteral("k"), true));
    QVERIFY(media.setProbeResult(QStringLiteral("k"), 9000, 1920, 1080));
    QVERIFY(media.favorite(QStringLiteral("k")));
    QVERIFY(media.watched(QStringLiteral("k")));
    QCOMPARE(media.positionMs(QStringLiteral("k")), 0);

    // thumbnails
    ThumbnailRepository thumbs(db);
    QVERIFY(thumbs.upsertOk(QStringLiteral("k"), 111, 222, QStringLiteral("/cache/t.png")));
    ThumbnailRepository::Record record;
    QVERIFY(thumbs.lookup(QStringLiteral("k"), &record));
    QCOMPARE(record.state, QStringLiteral("ok"));
    QCOMPARE(record.cachePath, QStringLiteral("/cache/t.png"));
    QVERIFY(thumbs.upsertFailed(QStringLiteral("bad"), 1, 2));
    QVERIFY(thumbs.upsertFailed(QStringLiteral("bad"), 1, 2));
    ThumbnailRepository::Record failed;
    QVERIFY(thumbs.lookup(QStringLiteral("bad"), &failed));
    QCOMPARE(failed.state, QStringLiteral("failed"));
    QCOMPARE(failed.failCount, 2);

    // recent
    RecentRepository recent(db);
    QVERIFY(recent.remember(QStringLiteral("/v")));
    QVERIFY(recent.remember(QStringLiteral("/v/sub")));
    const QList<QString> recents = recent.recent(10);
    QCOMPARE(recents.size(), 2);
    QCOMPARE(recents.first(), QStringLiteral("/v/sub"));

    // bookmarks
    BookmarkRepository bookmarks(db);
    QVERIFY(bookmarks.add(QStringLiteral("/v/x"), QStringLiteral("X")));
    QCOMPARE(bookmarks.paths(), QList<QString>{QStringLiteral("/v/x")});
    QVERIFY(bookmarks.remove(QStringLiteral("/v/x")));
    QVERIFY(bookmarks.paths().isEmpty());

    // settings + sort spec
    SettingsRepository settings(db);
    QVERIFY(settings.setValue(QStringLiteral("theme"), QStringLiteral("dark")));
    QCOMPARE(settings.value(QStringLiteral("theme")), QStringLiteral("dark"));

    SortSpec spec;
    spec.key = SortKey::Duration;
    spec.direction = SortDirection::Descending;
    spec.foldersFirst = false;
    QVERIFY(settings.setSortSpec(QStringLiteral("/v"), spec));
    const SortSpec restored = settings.sortSpec(QStringLiteral("/v"));
    QCOMPARE(restored.key, SortKey::Duration);
    QCOMPARE(restored.direction, SortDirection::Descending);
    QCOMPARE(restored.foldersFirst, false);

    // unknown folder yields the default sort
    const SortSpec defaulted = settings.sortSpec(QStringLiteral("/nowhere"));
    QCOMPARE(defaulted.key, SortKey::Name);
    QCOMPARE(defaulted.direction, SortDirection::Ascending);
}

void PersistenceTest::writeFailureIsReturnedWhenQueryOnly()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Database db;
    QVERIFY(db.open(dbPath(dir)));

    MediaStateRepository media(db);
    QVERIFY(media.setFavorite(QStringLiteral("id"), false));
    QVERIFY(db.execute(QStringLiteral("PRAGMA query_only = ON;")));

    const Result<void> failed = media.setFavorite(QStringLiteral("id"), true);
    QVERIFY(!failed);
    QCOMPARE(failed.error().code, ErrorCode::OperationFailed);
    QVERIFY(!failed.error().message.isEmpty());
}

QTEST_GUILESS_MAIN(PersistenceTest)
#include "test_persistence.moc"
