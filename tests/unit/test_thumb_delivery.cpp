// Thumbnail data-to-visual delivery (VIS-010).
//
// The chain under test is:
//   extraction (FFmpeg / freedesktop cache) -> ThumbnailController::applyResult
//   -> FolderModel::ThumbnailRole -> QML Image.source
//
// VIS-010 symptom: valid, probed video rows rendered no frame imagery. This
// suite pins down every hop so the first broken step cannot silently return.

#include <QtTest/QtTest>

#include <QDir>
#include <QFileInfo>
#include <QImage>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QUrl>

#include "controllers/thumbnail_controller.hpp"
#include "core/identity.hpp"
#include "media/thumbnails.hpp"
#include "models/folder_model.hpp"

using namespace qg;

namespace {

// Produces a real, decodable video with FFmpeg's testsrc. Returns an empty
// string when the CLI is unavailable so the caller can QSKIP.
QString makeSample(const QString &dir, const QString &name)
{
    const QString out = QDir(dir).filePath(name);
    QProcess ffmpeg;
    ffmpeg.start(QStringLiteral("ffmpeg"),
                 {QStringLiteral("-y"), QStringLiteral("-v"), QStringLiteral("error"),
                  QStringLiteral("-f"), QStringLiteral("lavfi"), QStringLiteral("-i"),
                  QStringLiteral("testsrc=duration=2:size=320x180:rate=10"),
                  QStringLiteral("-pix_fmt"), QStringLiteral("yuv420p"), out});
    const bool ok = ffmpeg.waitForStarted(5000) && ffmpeg.waitForFinished(60000)
                    && ffmpeg.exitCode() == 0;
    return ok && QFileInfo::exists(out) ? out : QString();
}

Entry entryFor(const QString &path)
{
    const QFileInfo info(path);
    Entry entry;
    entry.kind = EntryKind::Video;
    entry.name = info.fileName();
    entry.path = path;
    entry.mtimeNs = info.lastModified().toMSecsSinceEpoch() * 1000000LL;
    entry.size = info.size();
    const Result<FileIdentity> identity = identityFromPath(path);
    if (identity.hasValue())
        entry.identity = identity.value().key;
    return entry;
}

} // namespace

class ThumbDeliveryTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    void diagnoseDataToVisualChain();
    void modelRoleIsAnImageSourceUrl();
    void newlyExtractedThumbnailIsVisibleImagery();
    void freedesktopCacheHitSkipsExtraction();
    void failedExtractionSetsExplicitFailureState();
    void sourceChangeProducesANewCacheIdentity();
    void folderRowsAreNeverExtracted();
    void repeatedRequestVisibleDoesNotReExtract();

private:
    QTemporaryDir m_dir;
    QString m_mp4;
    QString m_mkv;
    QString m_cacheDir;
    QString m_fdHome; // empty on purpose: isolate from the host's cache
};

void ThumbDeliveryTest::initTestCase()
{
    QVERIFY(m_dir.isValid());
    m_mp4 = makeSample(m_dir.path(), QStringLiteral("sample.mp4"));
    m_mkv = makeSample(m_dir.path(), QStringLiteral("sample.mkv"));
    if (m_mp4.isEmpty())
        QSKIP("ffmpeg CLI unavailable; cannot build a real media fixture");

    m_cacheDir = QDir(m_dir.path()).filePath(QStringLiteral("qg-cache"));
    m_fdHome = QDir(m_dir.path()).filePath(QStringLiteral("empty-fd-cache"));
    QVERIFY(QDir().mkpath(m_cacheDir));
    QVERIFY(QDir().mkpath(m_fdHome));
}

// Logs every hop of the chain (VIS-010 evidence) and asserts the last one: what
// QML's Image.source would actually receive.
void ThumbDeliveryTest::diagnoseDataToVisualChain()
{
    const QString source = m_mp4;
    const Entry entry = entryFor(source);
    qInfo("VIS010 source      : %s", qUtf8Printable(source));
    qInfo("VIS010 exists=%d size=%lld mtimeNs=%lld",
          int(QFileInfo::exists(source)), entry.size, entry.mtimeNs);
    qInfo("VIS010 identity    : %s", qUtf8Printable(entry.identity));

    FolderModel model;
    FolderSnapshot snapshot;
    snapshot.entries = {entry};
    model.applySnapshot(snapshot);

    const QVariant before = model.data(model.index(0, 0), FolderModel::ThumbnailRole);
    qInfo("VIS010 role(before): type=%s valid=%d",
          before.typeName() ? before.typeName() : "null", int(before.isValid()));

    ThumbnailController controller;
    controller.setModel(&model);
    controller.setCacheDirectory(m_cacheDir);
    controller.setFreedesktopCacheHome(m_fdHome);
    controller.setThumbnailSize(320, 180);
    controller.setImmediateProcessing(true);

    QSignalSpy ready(&controller, &ThumbnailController::thumbnailReady);
    QSignalSpy failed(&controller, &ThumbnailController::thumbnailFailed);
    controller.requestVisible(0, 0);
    qInfo("VIS010 signals     : ready=%lld failed=%lld",
          static_cast<long long>(ready.count()), static_cast<long long>(failed.count()));

    const QVariant role = model.data(model.index(0, 0), FolderModel::ThumbnailRole);
    const int state = model.data(model.index(0, 0), FolderModel::ThumbnailStateRole).toInt();
    qInfo("VIS010 role(after) : type=%s state=%d",
          role.typeName() ? role.typeName() : "null", state);
    qInfo("VIS010 role->QUrl  : valid=%d url=%s",
          int(role.toUrl().isValid()), qUtf8Printable(role.toUrl().toString()));

    // The step that matters: can QML's Image.source consume this value?
    const QUrl url = role.toUrl();
    qInfo("VIS010 Image.source would be: '%s'", qUtf8Printable(url.toString()));
    if (url.isLocalFile()) {
        const QFileInfo file(url.toLocalFile());
        qInfo("VIS010 cache file  : %s exists=%d bytes=%lld",
              qUtf8Printable(file.absoluteFilePath()), int(file.exists()), file.size());
        QImage decoded;
        const bool loaded = decoded.load(file.absoluteFilePath());
        qInfo("VIS010 decodes     : %d size=%dx%d", int(loaded), decoded.width(),
              decoded.height());
    } else {
        qInfo("VIS010 cache file  : <not a local file URL>");
    }

    QCOMPARE(state, static_cast<int>(ThumbnailState::Ready));
    QVERIFY2(url.isValid() && url.isLocalFile(),
             "ThumbnailRole must expose a file URL that Image.source can load");
    QVERIFY(QFileInfo::exists(url.toLocalFile()));
    QImage decoded;
    QVERIFY2(decoded.load(url.toLocalFile()), "the exposed thumbnail must decode");
    QVERIFY(decoded.width() > 0 && decoded.height() > 0);
}

void ThumbDeliveryTest::modelRoleIsAnImageSourceUrl()
{
    FolderModel model;
    FolderSnapshot snapshot;
    snapshot.entries = {entryFor(m_mp4)};
    model.applySnapshot(snapshot);

    ThumbnailController controller;
    controller.setModel(&model);
    controller.setCacheDirectory(m_cacheDir);
    controller.setFreedesktopCacheHome(m_fdHome);
    controller.setImmediateProcessing(true);
    controller.requestVisible(0, 0);

    const QVariant role = model.data(model.index(0, 0), FolderModel::ThumbnailRole);
    const QUrl url = role.toUrl();
    QVERIFY2(url.isValid(), "role converts to a valid QUrl");
    QVERIFY2(url.isLocalFile(), "role is a local file URL");
    QVERIFY2(url.toString() == role.toString(),
             "string conversion preserves the URL (QML binds it directly)");
}

void ThumbDeliveryTest::newlyExtractedThumbnailIsVisibleImagery()
{
    for (const QString &path : {m_mp4, m_mkv}) {
        if (path.isEmpty())
            continue; // MKV sample optional
        FolderModel model;
        FolderSnapshot snapshot;
        snapshot.entries = {entryFor(path)};
        model.applySnapshot(snapshot);

        ThumbnailController controller;
        controller.setModel(&model);
        controller.setCacheDirectory(m_cacheDir);
        controller.setFreedesktopCacheHome(m_fdHome);
        controller.setThumbnailSize(320, 180);
        controller.setImmediateProcessing(true);
        controller.requestVisible(0, 0);

        QVERIFY2(model.data(model.index(0, 0), FolderModel::ThumbnailStateRole).toInt()
                     == static_cast<int>(ThumbnailState::Ready),
                 qPrintable(QStringLiteral("state Ready for %1").arg(QFileInfo(path).fileName())));

        const QUrl url = model.data(model.index(0, 0), FolderModel::ThumbnailRole).toUrl();
        QVERIFY2(url.isLocalFile(), "thumbnail URL present");
        QImage image;
        QVERIFY2(image.load(url.toLocalFile()), "thumbnail file decodes");
        QVERIFY(!image.isNull());
        QVERIFY(image.width() >= 16 && image.height() >= 9);
        // Real frame imagery, not a blank fill: the testsrc fixture is colourful.
        const QColor centre = image.pixelColor(image.width() / 2, image.height() / 2);
        QVERIFY2(centre.isValid(), "centre pixel readable");
    }
}

void ThumbDeliveryTest::freedesktopCacheHitSkipsExtraction()
{
    // Seed a valid freedesktop thumbnail (correct URI + MTime text) and assert
    // the controller serves it instead of decoding the video.
    const QString source = m_mp4;
    const QFileInfo info(source);
    const QString thumbPath = freedesktopThumbnailPath(m_fdHome, source, 256);
    QVERIFY(QDir().mkpath(QFileInfo(thumbPath).absolutePath()));
    {
        QImage seed(64, 36, QImage::Format_ARGB32);
        seed.fill(QColor(200, 30, 30));
        seed.setText(QStringLiteral("Thumb::URI"), freedesktopUri(source));
        seed.setText(QStringLiteral("Thumb::MTime"),
                     QString::number(entryFor(source).mtimeNs / 1000000000LL));
        QVERIFY(seed.save(thumbPath, "PNG"));
    }
    QVERIFY(QFileInfo::exists(thumbPath));

    // Cache-lookup evidence (VIS-010): what the finder sees for this source.
    const qint64 mtimeNs = entryFor(source).mtimeNs;
    const FreedesktopThumbnail probe = findFreedesktopThumbnail(m_fdHome, source, mtimeNs);
    qInfo("VIS010 fd uri      : %s", qUtf8Printable(freedesktopUri(source)));
    qInfo("VIS010 fd mtimeSec : %lld", static_cast<long long>(mtimeNs / 1000000000LL));
    qInfo("VIS010 fd seed path: %s exists=%d", qUtf8Printable(thumbPath),
          int(QFileInfo::exists(thumbPath)));
    {
        QImage reread;
        reread.load(thumbPath);
        qInfo("VIS010 fd seed text: uri='%s' mtime='%s'",
              qUtf8Printable(reread.text(QStringLiteral("Thumb::URI"))),
              qUtf8Printable(reread.text(QStringLiteral("Thumb::MTime"))));
    }
    qInfo("VIS010 fd lookup   : valid=%d path=%s", int(probe.valid),
          qUtf8Printable(probe.path));

    FolderModel model;
    FolderSnapshot snapshot;
    snapshot.entries = {entryFor(source)};
    model.applySnapshot(snapshot);

    ThumbnailController controller;
    controller.setModel(&model);
    controller.setCacheDirectory(m_cacheDir);
    controller.setFreedesktopCacheHome(m_fdHome);
    controller.setImmediateProcessing(true);

    // Any call to the extractor would prove the cache was bypassed.
    bool extractorCalled = false;
    controller.setExtractor([&extractorCalled](const ThumbnailRequest &) {
        extractorCalled = true;
        return ThumbnailExtraction{};
    });

    controller.requestVisible(0, 0);
    QCOMPARE(model.data(model.index(0, 0), FolderModel::ThumbnailStateRole).toInt(),
             static_cast<int>(ThumbnailState::Ready));
    QVERIFY2(!extractorCalled, "a freedesktop cache hit must not invoke the extractor");

    const QUrl url = model.data(model.index(0, 0), FolderModel::ThumbnailRole).toUrl();
    QVERIFY2(url.isLocalFile(), "cached thumbnail is exposed as a file URL");
    QImage served;
    QVERIFY(served.load(url.toLocalFile()));
    QCOMPARE(served.size(), QSize(64, 36)); // the seeded image, not a decode
}

void ThumbDeliveryTest::failedExtractionSetsExplicitFailureState()
{
    FolderModel model;
    FolderSnapshot snapshot;
    Entry entry = entryFor(m_mp4);
    entry.path = QDir(m_dir.path()).filePath(QStringLiteral("does-not-exist.mp4"));
    snapshot.entries = {entry};
    model.applySnapshot(snapshot);

    ThumbnailController controller;
    controller.setModel(&model);
    controller.setCacheDirectory(m_cacheDir);
    controller.setFreedesktopCacheHome(m_fdHome);
    controller.setImmediateProcessing(true);
    QSignalSpy failed(&controller, &ThumbnailController::thumbnailFailed);

    controller.requestVisible(0, 0);
    QCOMPARE(failed.count(), 1);
    QCOMPARE(model.data(model.index(0, 0), FolderModel::ThumbnailStateRole).toInt(),
             static_cast<int>(ThumbnailState::Failed));
    QVERIFY2(!model.data(model.index(0, 0), FolderModel::ThumbnailRole).isValid(),
             "a failed extraction must not expose a stale or bogus image");
}

void ThumbDeliveryTest::sourceChangeProducesANewCacheIdentity()
{
    const QString copy = QDir(m_dir.path()).filePath(QStringLiteral("mutable.mp4"));
    QFile::remove(copy);
    QVERIFY(QFile::copy(m_mp4, copy));

    FolderModel model;
    FolderSnapshot snapshot;
    snapshot.entries = {entryFor(copy)};
    model.applySnapshot(snapshot);

    ThumbnailController controller;
    controller.setModel(&model);
    controller.setCacheDirectory(m_cacheDir);
    controller.setFreedesktopCacheHome(m_fdHome);
    controller.setImmediateProcessing(true);
    controller.requestVisible(0, 0);

    const QString firstUrl =
        model.data(model.index(0, 0), FolderModel::ThumbnailRole).toUrl().toString();
    QVERIFY2(!firstUrl.isEmpty(), "first thumbnail URL present");

    // Rewrite the file (new mtime/size), re-enumerate, request again.
    QTest::qWait(20);
    QFile out(copy);
    QVERIFY(out.open(QIODevice::WriteOnly | QIODevice::Append));
    out.write(QByteArray(64, '\0'));
    out.close();

    FolderSnapshot refresh;
    refresh.entries = {entryFor(copy)};
    model.applySnapshot(refresh);
    controller.beginGeneration();
    controller.requestVisible(0, 0);

    const QString secondUrl =
        model.data(model.index(0, 0), FolderModel::ThumbnailRole).toUrl().toString();
    QVERIFY2(!secondUrl.isEmpty(), "thumbnail URL after source change");
    QVERIFY2(secondUrl != firstUrl,
             "a changed source must not reuse the previous thumbnail (no stale imagery)");
}

void ThumbDeliveryTest::folderRowsAreNeverExtracted()
{
    Entry folder;
    folder.kind = EntryKind::Folder;
    folder.name = QStringLiteral("Subfolder");
    folder.path = QDir(m_dir.path()).filePath(QStringLiteral("Subfolder"));
    folder.identity = QStringLiteral("inode:99:folder");
    const Entry video = entryFor(m_mp4);

    FolderModel model;
    FolderSnapshot snapshot;
    snapshot.entries = {folder, video};
    model.applySnapshot(snapshot);

    ThumbnailController controller;
    controller.setModel(&model);
    controller.setCacheDirectory(m_cacheDir);
    controller.setFreedesktopCacheHome(
        QDir(m_dir.path()).filePath(QStringLiteral("no-such-fd-cache")));
    controller.setImmediateProcessing(true);

    QList<QString> requested;
    controller.setExtractor([&requested](const ThumbnailRequest &request) {
        requested.append(request.path);
        return extractThumbnail(request.path, request.width, request.height);
    });

    controller.requestVisible(0, 1);
    QCOMPARE(requested.size(), 1); // the video only
    QCOMPARE(requested.first(), video.path);
    QCOMPARE(model.data(model.index(0, 0), FolderModel::ThumbnailStateRole).toInt(),
             static_cast<int>(ThumbnailState::None)); // folders stay untouched
    QCOMPARE(model.data(model.index(1, 0), FolderModel::ThumbnailStateRole).toInt(),
             static_cast<int>(ThumbnailState::Ready));
}

void ThumbDeliveryTest::repeatedRequestVisibleDoesNotReExtract()
{
    FolderModel model;
    FolderSnapshot snapshot;
    snapshot.entries = {entryFor(m_mp4)};
    model.applySnapshot(snapshot);

    ThumbnailController controller;
    controller.setModel(&model);
    controller.setCacheDirectory(m_cacheDir);
    // Isolated fd home: an earlier slot seeds a freedesktop thumbnail for the
    // same sample, and a cache hit would mask the re-extraction check.
    controller.setFreedesktopCacheHome(
        QDir(m_dir.path()).filePath(QStringLiteral("no-such-fd-cache")));
    controller.setImmediateProcessing(true);

    int extractions = 0;
    controller.setExtractor([&extractions](const ThumbnailRequest &request) {
        ++extractions;
        return extractThumbnail(request.path, request.width, request.height);
    });

    controller.requestVisible(0, 0);
    controller.requestVisible(0, 0);
    controller.requestVisible(0, 0);
    QCOMPARE(extractions, 1); // settled rows are never re-extracted (§18.4)
}

QTEST_GUILESS_MAIN(ThumbDeliveryTest)
#include "test_thumb_delivery.moc"
