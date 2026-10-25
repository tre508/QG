// Media probe tests (Prompt 28).

#include <QtTest/QtTest>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "media/probe.hpp"
#include "persistence/database.hpp"

using namespace qg;

class ProbeTest : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void probesDurationResolutionAndTracks();
    void malformedInputFailsGracefully();
    void serviceServesSecondRequestFromCache();

private:
    QTemporaryDir m_dir;
    QString m_sample;
};

void ProbeTest::initTestCase()
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
        QSKIP("ffmpeg CLI unavailable; skipping probe fixture");
    }
}

void ProbeTest::probesDurationResolutionAndTracks()
{
    const ProbeResult result = probeMedia(m_sample);
    QVERIFY2(result.ok(), qPrintable(result.error.message));
    QCOMPARE(result.width, 128);
    QCOMPARE(result.height, 96);
    QVERIFY(result.durationMs >= 800 && result.durationMs <= 1200);
}

void ProbeTest::malformedInputFailsGracefully()
{
    const QString junk = QDir(m_dir.path()).filePath(QStringLiteral("junk.mp4"));
    QFile file(junk);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("definitely not a container");
    file.close();

    const ProbeResult result = probeMedia(junk);
    QVERIFY(!result.ok());
    QVERIFY(result.error.isError());
}

void ProbeTest::serviceServesSecondRequestFromCache()
{
    QTemporaryDir dbDir;
    QVERIFY(dbDir.isValid());
    Database db;
    QVERIFY(db.open(QDir(dbDir.path()).filePath(QStringLiteral("qg.db"))));

    int proberCalls = 0;
    MediaProbeService service;
    service.setDatabase(&db);
    service.setImmediateProcessing(true);
    service.setProber([&proberCalls](const QString &path) {
        ++proberCalls;
        return probeMedia(path);
    });

    Entry entry;
    entry.kind = EntryKind::Video;
    entry.name = QStringLiteral("sample.mp4");
    entry.path = m_sample;
    entry.identity = QStringLiteral("inode:1:2");
    entry.size = QFileInfo(m_sample).size();
    entry.mtimeNs = 123456789;

    QSignalSpy ready(&service, &MediaProbeService::probeReady);
    service.request(entry);
    QCOMPARE(ready.count(), 1);
    QCOMPARE(proberCalls, 1);

    // Second request with the same source: served from cache, no re-probe.
    service.request(entry);
    QCOMPARE(ready.count(), 2);
    QCOMPARE(proberCalls, 1);
    QVERIFY(ready.takeLast().at(1).toLongLong() > 0); // duration carried
}

QTEST_GUILESS_MAIN(ProbeTest)
#include "test_probe.moc"
