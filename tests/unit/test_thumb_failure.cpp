// Thumbnail failure / retry / invalidation tests (Prompt 26).

#include <QtTest/QtTest>

#include <QDir>
#include <QTemporaryDir>

#include "media/thumbnails.hpp"
#include "persistence/database.hpp"
#include "persistence/repositories.hpp"

using namespace qg;

class ThumbFailureTest : public QObject
{
    Q_OBJECT

private slots:
    void missingEntryGenerates();
    void okCurrentIsServed();
    void staleOkRegenerates();
    void failedRetriesAreBounded();
    void sourceChangeResetsTerminal();
    void manualRetryOverridesTerminal();
    void repositoryMarkerIncrementsFailCount();
};

void ThumbFailureTest::missingEntryGenerates()
{
    ThumbnailCacheState cache; // present == false
    QCOMPARE(decideThumbnail(cache, 100, 200), ThumbnailAction::Generate);
}

void ThumbFailureTest::okCurrentIsServed()
{
    ThumbnailCacheState cache;
    cache.present = true;
    cache.state = QStringLiteral("ok");
    cache.sourceMtimeNs = 100;
    cache.sourceSize = 200;
    QCOMPARE(decideThumbnail(cache, 100, 200), ThumbnailAction::Serve);
}

void ThumbFailureTest::staleOkRegenerates()
{
    ThumbnailCacheState cache;
    cache.present = true;
    cache.state = QStringLiteral("ok");
    cache.sourceMtimeNs = 100;
    cache.sourceSize = 200;
    QCOMPARE(decideThumbnail(cache, 100, 999), ThumbnailAction::Generate); // size changed
    QCOMPARE(decideThumbnail(cache, 555, 200), ThumbnailAction::Generate); // mtime changed
}

void ThumbFailureTest::failedRetriesAreBounded()
{
    ThumbnailCacheState cache;
    cache.present = true;
    cache.state = QStringLiteral("failed");
    cache.sourceMtimeNs = 100;
    cache.sourceSize = 200;

    cache.failCount = 1;
    QCOMPARE(decideThumbnail(cache, 100, 200), ThumbnailAction::Retry);
    cache.failCount = 2;
    QCOMPARE(decideThumbnail(cache, 100, 200), ThumbnailAction::Retry);
    cache.failCount = 3; // two retries consumed -> terminal
    QCOMPARE(decideThumbnail(cache, 100, 200), ThumbnailAction::Terminal);
}

void ThumbFailureTest::sourceChangeResetsTerminal()
{
    ThumbnailCacheState cache;
    cache.present = true;
    cache.state = QStringLiteral("failed");
    cache.failCount = 9;
    cache.sourceMtimeNs = 100;
    cache.sourceSize = 200;

    QCOMPARE(decideThumbnail(cache, 100, 200), ThumbnailAction::Terminal);
    QCOMPARE(decideThumbnail(cache, 300, 200), ThumbnailAction::Retry); // mtime changed
}

void ThumbFailureTest::manualRetryOverridesTerminal()
{
    ThumbnailCacheState cache;
    cache.present = true;
    cache.state = QStringLiteral("failed");
    cache.failCount = 9;
    cache.sourceMtimeNs = 100;
    cache.sourceSize = 200;

    QCOMPARE(decideThumbnail(cache, 100, 200, 2, /*manualRetry=*/true), ThumbnailAction::Retry);
}

void ThumbFailureTest::repositoryMarkerIncrementsFailCount()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Database db;
    QVERIFY(db.open(QDir(dir.path()).filePath(QStringLiteral("qg.db"))));

    ThumbnailRepository repo(db);
    QVERIFY(repo.upsertFailed(QStringLiteral("inode:1:2"), 100, 200));
    QVERIFY(repo.upsertFailed(QStringLiteral("inode:1:2"), 100, 200));

    ThumbnailRepository::Record record;
    QVERIFY(repo.lookup(QStringLiteral("inode:1:2"), &record));
    QCOMPARE(record.state, QStringLiteral("failed"));
    QCOMPARE(record.failCount, 2);
    QCOMPARE(record.sourceMtimeNs, 100);
    QCOMPARE(record.sourceSize, 200);
}

QTEST_GUILESS_MAIN(ThumbFailureTest)
#include "test_thumb_failure.moc"
