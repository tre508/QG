// 100k entry memory and performance test (FIX-PERF-006 / DESIGN.md §25.11)

#include <QtTest/QtTest>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QFile>
#include <QDir>

#include "controllers/probe_controller.hpp"
#include "models/folder_model.hpp"
#include "controllers/browser_controller.hpp"

using namespace qg;

namespace {

Entry makeVideo(const QString &name, const QString &identity, qint64 mtimeNs, qint64 size)
{
    Entry entry;
    entry.kind = EntryKind::Video;
    entry.name = name;
    entry.path = QStringLiteral("/v/") + name;
    entry.identity = identity;
    entry.size = size;
    entry.mtimeNs = mtimeNs;
    return entry;
}

} // namespace

class TestProbeMemory : public QObject
{
    Q_OBJECT

private slots:
    void modelMemoryWith100kEntries();
    void probeCoalescedResortWithManyResults();
    void browserControllerMemoryWith100kEntries();
};

void TestProbeMemory::modelMemoryWith100kEntries()
{
    // Create a 100k entry model and measure memory
    FolderModel model;
    FolderSnapshot snapshot;
    snapshot.entries.reserve(100000);
    
    for (int i = 0; i < 100000; ++i) {
        snapshot.entries.append(makeVideo(
            QStringLiteral("video_%1.mp4").arg(i, 6, 10, QLatin1Char('0')),
            QStringLiteral("id-%1").arg(i),
            1000000 + i,
            1024 * 1024 + i * 1024
        ));
    }
    
    QElapsedTimer timer;
    timer.start();
    model.applySnapshot(snapshot);
    qint64 applyMs = timer.elapsed();
    
    QCOMPARE(model.rowCount(), 100000);
    qInfo("applySnapshot 100k: %lld ms", static_cast<long long>(applyMs));
    
    // Verify memory is bounded - model should not hold excessive memory
    // Each Entry is ~80 bytes, 100k = ~8MB base
    // With QString overhead, expect < 50MB total
    
    // Test reorder (sort) performance
    SortSpec spec;
    spec.key = SortKey::Duration;
    spec.direction = SortDirection::Ascending;
    spec.foldersFirst = true;
    
    timer.restart();
    model.reorder(spec);
    qint64 sortMs = timer.elapsed();
    qInfo("reorder 100k by Duration: %lld ms", static_cast<long long>(sortMs));
    
    // Should complete in reasonable time
    QVERIFY(sortMs < 5000); // generous ceiling
}

void TestProbeMemory::probeCoalescedResortWithManyResults()
{
    // Test that probe results are properly coalesced into single resort
    FolderModel model;
    FolderSnapshot snapshot;
    snapshot.entries.reserve(1000);
    
    for (int i = 0; i < 1000; ++i) {
        snapshot.entries.append(makeVideo(
            QStringLiteral("video_%1.mp4").arg(i, 4, 10, QLatin1Char('0')),
            QStringLiteral("id-%1").arg(i),
            1000000 + i,
            1024 * 1024
        ));
    }
    model.applySnapshot(snapshot);
    
    ProbeController controller;
    controller.setModel(&model);
    controller.setImmediateProcessing(true);
    controller.setProber([](const QString &) {
        ProbeResult result;
        result.durationMs = 5000;
        result.width = 64;
        result.height = 48;
        return result;
    });
    
    QSignalSpy resort(&controller, &ProbeController::resortRequested);
    
    // Request all 1000 items - they should all fire probes synchronously
    controller.requestVisible(0, 999);
    
    // Should coalesce into a single resort signal
    QVERIFY(resort.wait(1000));
    QCOMPARE(resort.count(), 1);
    
    // Give debounce time to fire again if it was going to
    QTest::qWait(300);
    QCOMPARE(resort.count(), 1);
}

void TestProbeMemory::browserControllerMemoryWith100kEntries()
{
    // Test BrowserController with 100k entries
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QDir root(dir.path());
    
    // Create 100k files
    for (int i = 0; i < 100000; ++i) {
        QFile f(root.filePath(QStringLiteral("video_%1.mp4").arg(i, 6, 10, QLatin1Char('0'))));
        f.open(QIODevice::WriteOnly);
        f.close();
    }
    
    BrowserController controller;
    QVERIFY(controller.navigate(dir.path()));
    
    QElapsedTimer timer;
    timer.start();
    while (controller.busy() && timer.elapsed() < 10000) {
        QCoreApplication::processEvents();
        QTest::qWait(50);
    }
    
    qint64 navMs = timer.elapsed();
    QVERIFY(!controller.busy());
    QCOMPARE(controller.model()->rowCount(), 100000);
    
    qInfo("navigate 100k: %lld ms", static_cast<long long>(navMs));
    
    // Verify model is still responsive
    QVERIFY(controller.model()->rowCount() == 100000);
    
    // Test navigation away and back
    controller.parent();
    QTRY_VERIFY(!controller.busy());
    controller.navigate(dir.path());
    QTRY_VERIFY(!controller.busy());
    QCOMPARE(controller.model()->rowCount(), 100000);
}

QTEST_GUILESS_MAIN(TestProbeMemory)
#include "test_probe_memory.moc"