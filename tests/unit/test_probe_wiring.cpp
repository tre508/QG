// Probe → model wiring tests (Prompt 32).

#include <QtTest/QtTest>

#include <QSignalSpy>

#include "controllers/probe_controller.hpp"
#include "models/folder_model.hpp"

using namespace qg;

namespace {

Entry makeVideo(const QString &name, const QString &identity, qint64 mtimeNs, qint64 size)
{
    Entry entry;
    entry.kind = EntryKind::Video;
    entry.name = name;
    entry.path = QStringLiteral("/v/") + name;
    entry.identity = identity;
    entry.mtimeNs = mtimeNs;
    entry.size = size;
    return entry;
}

} // namespace

class ProbeWiringTest : public QObject
{
    Q_OBJECT

private slots:
    void appliesProbeResultsToDurationRoles();
    void unprobedItemsRemainUnknown();
    void probeBurstCoalescesIntoSingleResort();
    void repeatedRequestVisibleDoesNotReProbe();
};

void ProbeWiringTest::appliesProbeResultsToDurationRoles()
{
    FolderModel model;
    FolderSnapshot snapshot;
    snapshot.entries = {makeVideo(QStringLiteral("a.mp4"), QStringLiteral("ia"), 100, 10),
                        makeVideo(QStringLiteral("b.mp4"), QStringLiteral("ib"), 100, 20)};
    model.applySnapshot(snapshot);

    ProbeController controller;
    controller.setModel(&model);
    controller.setImmediateProcessing(true);
    controller.setProber([](const QString &) {
        ProbeResult result;
        result.durationMs = 9000;
        result.width = 128;
        result.height = 96;
        return result;
    });

    QSignalSpy applied(&controller, &ProbeController::probeApplied);
    controller.requestVisible(0, 1);

    QCOMPARE(applied.count(), 2);

    QCOMPARE(model.data(model.index(0, 0), FolderModel::DurationRole).toLongLong(), 9000LL);
    QCOMPARE(model.data(model.index(0, 0), FolderModel::WidthRole).toInt(), 128);
    QCOMPARE(model.data(model.index(0, 0), FolderModel::HeightRole).toInt(), 96);
    QCOMPARE(model.data(model.index(1, 0), FolderModel::DurationRole).toLongLong(), 9000LL);
}

void ProbeWiringTest::unprobedItemsRemainUnknown()
{
    FolderModel model;
    FolderSnapshot snapshot;
    snapshot.entries = {makeVideo(QStringLiteral("a.mp4"), QStringLiteral("ia"), 100, 10),
                        makeVideo(QStringLiteral("b.mp4"), QStringLiteral("ib"), 100, 20),
                        makeVideo(QStringLiteral("c.mp4"), QStringLiteral("ic"), 100, 30)};
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

    // Only the first two are requested; the third stays unprobed.
    controller.requestVisible(0, 1);

    QVERIFY(model.data(model.index(0, 0), FolderModel::DurationRole).isValid());
    QVERIFY(model.data(model.index(1, 0), FolderModel::DurationRole).isValid());
    // Unprobed item: no duration yet (renders as "--:--" and sorts last).
    QVERIFY(!model.data(model.index(2, 0), FolderModel::DurationRole).isValid());
}

void ProbeWiringTest::probeBurstCoalescesIntoSingleResort()
{
    FolderModel model;
    FolderSnapshot snapshot;
    snapshot.entries = {makeVideo(QStringLiteral("a.mp4"), QStringLiteral("ia"), 100, 10),
                        makeVideo(QStringLiteral("b.mp4"), QStringLiteral("ib"), 100, 20),
                        makeVideo(QStringLiteral("c.mp4"), QStringLiteral("ic"), 100, 30)};
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
    controller.requestVisible(0, 2); // three probe results land synchronously

    // Exactly one coalesced resort must fire (not one per probe).
    QVERIFY(resort.wait(500));
    QCOMPARE(resort.count(), 1);

    // Give the debounce a chance to (wrongly) fire a second time, then assert.
    QTest::qWait(250);
    QCOMPARE(resort.count(), 1);
}

// REM-01: the ContentView re-reports its viewport on every scroll/resize; rows
// already probed (duration known) or already requested this generation must not
// be re-probed.
void ProbeWiringTest::repeatedRequestVisibleDoesNotReProbe()
{
    FolderModel model;
    FolderSnapshot snapshot;
    snapshot.entries = {makeVideo(QStringLiteral("a.mp4"), QStringLiteral("ia"), 100, 10),
                        makeVideo(QStringLiteral("b.mp4"), QStringLiteral("ib"), 100, 20)};
    model.applySnapshot(snapshot);

    ProbeController controller;
    controller.setModel(&model);
    controller.setImmediateProcessing(true);
    int probes = 0;
    controller.setProber([&probes](const QString &) {
        ++probes;
        ProbeResult result;
        result.durationMs = 7000;
        result.width = 64;
        result.height = 48;
        return result;
    });

    controller.requestVisible(0, 1);
    QCOMPARE(probes, 2);

    // Re-reporting the same window: both rows now have a duration → no work.
    controller.requestVisible(0, 1);
    QCOMPARE(probes, 2);

    // Navigation resets the model, so a fresh viewport re-probes.
    model.applySnapshot(snapshot);
    controller.beginGeneration();
    controller.requestVisible(0, 1);
    QCOMPARE(probes, 4);
}

QTEST_GUILESS_MAIN(ProbeWiringTest)
#include "test_probe_wiring.moc"