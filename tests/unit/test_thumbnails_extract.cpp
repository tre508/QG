// Thumbnail extraction tests (Prompt 24).

#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>

#include "media/thumbnails.hpp"

using namespace qg;

class ThumbnailExtractTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void extractsFrameAtRequestedSize();
    void invalidFileFailsCleanly();

private:
    QTemporaryDir m_dir;
    QString m_sample;
};

void ThumbnailExtractTest::initTestCase()
{
    QVERIFY(m_dir.isValid());
    m_sample = QDir(m_dir.path()).filePath(QStringLiteral("sample.mp4"));

    QProcess ffmpeg;
    ffmpeg.start(QStringLiteral("ffmpeg"),
                 {QStringLiteral("-y"), QStringLiteral("-f"), QStringLiteral("lavfi"),
                  QStringLiteral("-i"), QStringLiteral("testsrc=duration=1:size=128x96:rate=5"),
                  QStringLiteral("-pix_fmt"), QStringLiteral("yuv420p"), m_sample});
    const bool ok = ffmpeg.waitForStarted(5000) && ffmpeg.waitForFinished(60000)
                    && ffmpeg.exitCode() == 0;
    if (!ok) {
        QSKIP("ffmpeg CLI unavailable; skipping extraction fixture");
    }
    QVERIFY2(QFileInfo::exists(m_sample), "sample video was not produced");
}

void ThumbnailExtractTest::extractsFrameAtRequestedSize()
{
    const ThumbnailExtraction result = extractThumbnail(m_sample, 160, 90);
    QVERIFY2(result.ok(), qPrintable(result.error.message));
    QCOMPARE(result.image.width(), 160);
    QCOMPARE(result.image.height(), 90);
    QVERIFY(result.sourceSize > 0);
    QVERIFY(result.sourceMtimeNs > 0);
}

void ThumbnailExtractTest::invalidFileFailsCleanly()
{
    const QString junk = QDir(m_dir.path()).filePath(QStringLiteral("junk.mp4"));
    QFile file(junk);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("this is not a video");
    file.close();

    const ThumbnailExtraction result = extractThumbnail(junk, 160, 90);
    QVERIFY(!result.ok());
    QVERIFY(result.error.isError());
}

QTEST_GUILESS_MAIN(ThumbnailExtractTest)
#include "test_thumbnails_extract.moc"
