// Thumbnail cache + freedesktop reader tests (Prompt 23).

#include <QtTest/QtTest>

#include <QDir>
#include <QImage>
#include <QTemporaryDir>

#include "media/thumbnails.hpp"
#include "persistence/database.hpp"
#include "persistence/repositories.hpp"

using namespace qg;

class ThumbnailsTest : public QObject
{
    Q_OBJECT

private slots:
    void cacheKeyChangesWithSource();
    void freedesktopValidFixtureAccepted();
    void freedesktopMtimeMismatchRejected();
    void freedesktopMissingIsInvalid();
    void repositoryRowRoundTrip();
};

void ThumbnailsTest::cacheKeyChangesWithSource()
{
    ThumbnailKey base;
    base.identity = QStringLiteral("inode:1:2");
    base.mtimeNs = 1000;
    base.size = 500;

    ThumbnailKey newer = base;
    newer.mtimeNs = 2000;

    ThumbnailKey larger = base;
    larger.size = 900;

    const QString root = QStringLiteral("/cache/qg");
    QVERIFY(qgThumbnailPath(root, base) != qgThumbnailPath(root, newer));
    QVERIFY(qgThumbnailPath(root, base) != qgThumbnailPath(root, larger));
    QCOMPARE(qgThumbnailPath(root, base), qgThumbnailPath(root, base));
    QVERIFY(qgThumbnailPath(root, base).endsWith(QStringLiteral(".png")));
}

namespace {

QString writeFreedesktopFixture(const QTemporaryDir &dir,
                                const QString &sourcePath,
                                qint64 mtimeNs,
                                int bucket)
{
    const QString thumbPath = freedesktopThumbnailPath(dir.path(), sourcePath, bucket);
    QDir().mkpath(QFileInfo(thumbPath).absolutePath());

    QImage image(64, 64, QImage::Format_ARGB32);
    image.fill(Qt::red);
    image.setText(QStringLiteral("Thumb::URI"), freedesktopUri(sourcePath));
    image.setText(QStringLiteral("Thumb::MTime"), QString::number(mtimeNs / 1000000000LL));
    const bool saved = image.save(thumbPath, "PNG");
    return saved ? thumbPath : QString();
}

} // namespace

void ThumbnailsTest::freedesktopValidFixtureAccepted()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString source = QDir(dir.path()).filePath(QStringLiteral("clip.mp4"));
    const qint64 mtimeNs = 1717000000000000000LL;

    const QString thumbPath = writeFreedesktopFixture(dir, source, mtimeNs, 256);
    QVERIFY(!thumbPath.isEmpty());

    const FreedesktopThumbnail found = findFreedesktopThumbnail(dir.path(), source, mtimeNs, {256});
    QVERIFY(found.valid);
    QCOMPARE(found.path, thumbPath);
    QVERIFY(!found.image.isNull());
}

void ThumbnailsTest::freedesktopMtimeMismatchRejected()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString source = QDir(dir.path()).filePath(QStringLiteral("clip.mp4"));
    const qint64 mtimeNs = 1717000000000000000LL;

    QVERIFY(!writeFreedesktopFixture(dir, source, mtimeNs, 256).isEmpty());

    // Source changed since the thumbnail was made: reject.
    const FreedesktopThumbnail found =
        findFreedesktopThumbnail(dir.path(), source, mtimeNs + 1000000000LL, {256});
    QVERIFY(!found.valid);
}

void ThumbnailsTest::freedesktopMissingIsInvalid()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString source = QDir(dir.path()).filePath(QStringLiteral("clip.mp4"));
    const FreedesktopThumbnail found =
        findFreedesktopThumbnail(dir.path(), source, 123, {128, 256, 512});
    QVERIFY(!found.valid);
}

void ThumbnailsTest::repositoryRowRoundTrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    Database db;
    QVERIFY(db.open(QDir(dir.path()).filePath(QStringLiteral("qg.db"))));

    ThumbnailRepository repo(db);
    QVERIFY(repo.upsertOk(QStringLiteral("inode:1:2"), 1000, 500, QStringLiteral("/cache/x.png")));

    ThumbnailRepository::Record record;
    QVERIFY(repo.lookup(QStringLiteral("inode:1:2"), &record));
    QCOMPARE(record.state, QStringLiteral("ok"));
    QCOMPARE(record.cachePath, QStringLiteral("/cache/x.png"));
}

QTEST_GUILESS_MAIN(ThumbnailsTest)
#include "test_thumbnails.moc"
