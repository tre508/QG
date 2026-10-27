// AUD-003: every file operation must run on the bounded I/O pool, never on the
// GUI thread (DESIGN.md §17.6, §15.3, §22.1 rules 1 and 4).
//
// The audit's claim was that the Q_INVOKABLE entry points performed their work
// inline — `emitFinished(kind, renameSync(...))` — so a bulk copy froze the UI
// and the result signal re-entered QML before the call returned. These cases pin
// the contract the fix has to keep:
//   * the Q_INVOKABLE call returns before the result is delivered (asynchrony),
//     and the delivery lands on the GUI thread, exactly once;
//   * the GUI event loop keeps running while the work is in flight (the 16 ms
//     responsiveness gate from the audit's deferred verification);
//   * the *Sync helpers stay usable as the payload (tests call them directly);
//   * a service destroyed while its work is in flight is never delivered to.
//
// The discriminating assertions are deliberately ordered. In
// `resultIsDeliveredExactlyOnceAfterTheEventLoopTurns` the delivery hop is a
// queued invocation, so `delivered == 0` immediately after the call cannot race:
// it is true only if the work was not done inline. In the watchdog case,
// `ticks > 0` after the wait is only reachable if the loop was free to turn while
// the copy was running. With the pre-fix code the call blocks (so `delivered` is
// already 1) and no timer can fire, so both go red.

#include "filesystem/operations.hpp"

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QTest>
#include <QThread>
#include <QTimer>

namespace {

void writeFile(const QString &path, int size = 16)
{
    QFile f(path);
    QVERIFY2(f.open(QIODevice::WriteOnly), qPrintable(path));
    f.write(QByteArray(size, 'q'));
    f.close();
}

// `files` small videos in `root` — enough that a copy is measurable but fast.
int createTree(const QString &root, int files, int fileSize)
{
    if (!QDir().mkpath(root)) {
        return 0;
    }
    QDir dir(root);
    for (int i = 0; i < files; ++i) {
        const QString name = QStringLiteral("clip-%1.mp4").arg(i, 5, 10, QLatin1Char('0'));
        QFile f(dir.filePath(name));
        if (!f.open(QIODevice::WriteOnly)) {
            return i;
        }
        f.write(QByteArray(fileSize, 'q'));
        f.close();
    }
    return files;
}

int countFiles(const QString &root)
{
    return QDir(root).entryList(QDir::Files | QDir::NoDotAndDotDot).size();
}

} // namespace

class FileOpsAsyncTest : public QObject {
    Q_OBJECT

private slots:
    void resultIsDeliveredExactlyOnceAfterTheEventLoopTurns();
    void resultArrivesOnTheGuiThread();
    void guiEventLoopKeepsRunningWhileABulkCopyIsInFlight();
    void everyMutatingEntryPointIsRoutedThroughThePool();
    void syncHelpersStayUsableAndAreStillThePayload();
    void serviceDestroyedInFlightIsNeverDeliveredTo();
};

// The delivery hop is a queued invocation, so nothing can reach the receiver
// while this test function is still running: `delivered == 0` immediately after
// the call is a deterministic proof that the work was not done inline, whatever
// the pool's threads happen to be doing.
void FileOpsAsyncTest::resultIsDeliveredExactlyOnceAfterTheEventLoopTurns()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString source = dir.filePath(QStringLiteral("a.mp4"));
    writeFile(source);

    FileOperationsService ops;
    int delivered = 0;
    QString deliveredKind;
    OperationResult deliveredResult;
    QObject::connect(&ops, &FileOperationsService::operationFinished, &ops,
                     [&](const QString &kind, const OperationResult &result) {
                         ++delivered;
                         deliveredKind = kind;
                         deliveredResult = result;
                     });

    ops.rename(source, QStringLiteral("renamed"));

    QCOMPARE(delivered, 0); // pre-fix: the signal already fired (synchronous)
    QTRY_COMPARE(delivered, 1);
    QTest::qWait(50); // nothing else may follow
    QCOMPARE(delivered, 1);
    QCOMPARE(deliveredKind, QStringLiteral("rename"));
    QVERIFY(deliveredResult.success);
    QCOMPARE(deliveredResult.completedPath, dir.filePath(QStringLiteral("renamed.mp4")));
    QVERIFY(QFileInfo::exists(dir.filePath(QStringLiteral("renamed.mp4"))));
    QVERIFY(!QFileInfo::exists(source));
}

void FileOpsAsyncTest::resultArrivesOnTheGuiThread()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString source = dir.filePath(QStringLiteral("a.mp4"));
    writeFile(source);

    const Qt::HANDLE guiThread = QThread::currentThreadId();

    FileOperationsService ops;
    Qt::HANDLE deliveringThread = nullptr;
    QObject::connect(&ops, &FileOperationsService::operationFinished, &ops,
                     [&](const QString &, const OperationResult &) {
                         deliveringThread = QThread::currentThreadId();
                     });

    ops.copy({source}, dir.path());
    QTRY_VERIFY(deliveringThread != nullptr);
    QCOMPARE(deliveringThread, guiThread);
    QCOMPARE(QThread::currentThread(), QCoreApplication::instance()->thread());
}

// The audit's deferred verification: "assert the UI stays responsive, and
// measure GUI-thread block time". A 1 ms watchdog counts event-loop turns: the
// work is on the I/O pool, so the loop runs while the copy is in flight. With
// the pre-fix code the copy completes inside the call and nothing ticks.
void FileOpsAsyncTest::guiEventLoopKeepsRunningWhileABulkCopyIsInFlight()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString sourceDir = dir.filePath(QStringLiteral("tree"));
    const QString targetDir = dir.filePath(QStringLiteral("target"));
    QVERIFY(QDir().mkpath(targetDir));
    // ~3 MB in 400 files: the copy measures ~800 ms on this host, so "the call
    // returned at once" and "the loop kept turning" both mean something. The
    // same fixture blocks the GUI thread for that whole time pre-fix.
    const int expected = 400;
    QCOMPARE(createTree(sourceDir, expected, 8192), expected);

    FileOperationsService ops;
    int delivered = 0;
    QObject::connect(&ops, &FileOperationsService::operationFinished, &ops,
                     [&](const QString &, const OperationResult &) { ++delivered; });

    QTimer watchdog;
    watchdog.setInterval(1);
    int ticks = 0;
    QObject::connect(&watchdog, &QTimer::timeout, &ops, [&]() { ++ticks; });
    watchdog.start();

    QElapsedTimer callTimer;
    callTimer.start();
    ops.copy({sourceDir}, targetDir);
    const qint64 callMs = callTimer.elapsed();

    // Submitting to a pool is a bookkeeping operation, not a copy.
    QVERIFY2(callMs < 100, qPrintable(QStringLiteral("the Q_INVOKABLE blocked for %1 ms")
                                          .arg(callMs)));
    QCOMPARE(delivered, 0);

    QTRY_COMPARE_WITH_TIMEOUT(delivered, 1, 30000);
    watchdog.stop();

    QVERIFY2(ticks > 0, "the GUI event loop never turned while the copy was in flight");
    QCOMPARE(countFiles(targetDir + QDir::separator() + QStringLiteral("tree")), expected);
}

void FileOpsAsyncTest::everyMutatingEntryPointIsRoutedThroughThePool()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString targetDir = dir.filePath(QStringLiteral("target"));
    QVERIFY(QDir().mkpath(targetDir));

    FileOperationsService ops;
    int delivered = 0;
    QStringList kinds;
    QObject::connect(&ops, &FileOperationsService::operationFinished, &ops,
                     [&](const QString &kind, const OperationResult &) {
                         ++delivered;
                         kinds.append(kind);
                     });

    const QString copiable = dir.filePath(QStringLiteral("copy-me.mp4"));
    writeFile(copiable);
    ops.copy({copiable}, targetDir);
    QCOMPARE(delivered, 0);
    QTRY_COMPARE(delivered, 1);

    const QString movable = dir.filePath(QStringLiteral("move-me.mp4"));
    writeFile(movable);
    ops.move({movable}, targetDir);
    QCOMPARE(delivered, 1);
    QTRY_COMPARE(delivered, 2);

    const QString doomed = dir.filePath(QStringLiteral("doomed.mp4"));
    writeFile(doomed);
    ops.deletePermanently({doomed});
    QCOMPARE(delivered, 2);
    QTRY_COMPARE(delivered, 3);

    QCOMPARE(kinds, QStringList({QStringLiteral("copy"), QStringLiteral("move"),
                                 QStringLiteral("delete")}));
    QVERIFY(QFileInfo::exists(targetDir + QDir::separator() + QStringLiteral("copy-me.mp4")));
    QVERIFY(QFileInfo::exists(targetDir + QDir::separator() + QStringLiteral("move-me.mp4")));
    QVERIFY(!QFileInfo::exists(doomed));
}

// The *Sync helpers are the payload of the facade and are called directly by the
// existing suites, so routing the entry points through the pool must not change
// them.
void FileOpsAsyncTest::syncHelpersStayUsableAndAreStillThePayload()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString targetDir = dir.filePath(QStringLiteral("target"));
    QVERIFY(QDir().mkpath(targetDir));

    const QString source = dir.filePath(QStringLiteral("a.mp4"));
    writeFile(source);

    const OperationResult renamed =
        FileOperationsService::renameSync(source, QStringLiteral("b"));
    QVERIFY(renamed.success);
    QCOMPARE(renamed.completedPath, dir.filePath(QStringLiteral("b.mp4")));

    const OperationResult copied =
        FileOperationsService::copySync({renamed.completedPath}, targetDir);
    QVERIFY(copied.success);

    const OperationResult moved =
        FileOperationsService::moveSync({renamed.completedPath}, targetDir);
    QVERIFY(moved.success);

    const OperationResult removed = FileOperationsService::deletePermanentlySync(
        {targetDir + QDir::separator() + QStringLiteral("b.mp4")});
    QVERIFY(removed.success);

    // A missing source is still an explicit failure, never a phantom success.
    const OperationResult ghost =
        FileOperationsService::deletePermanentlySync({dir.filePath(QStringLiteral("nope.mp4"))});
    QVERIFY(!ghost.success);
}

// The receiver is only a lifetime token: a service destroyed while its result is
// in flight must not be delivered to (and must not crash). The work itself is
// unaffected, so the copy still completes on the pool.
void FileOpsAsyncTest::serviceDestroyedInFlightIsNeverDeliveredTo()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString sourceDir = dir.filePath(QStringLiteral("tree"));
    const QString targetDir = dir.filePath(QStringLiteral("target"));
    QVERIFY(QDir().mkpath(targetDir));
    const int expected = 150;
    QCOMPARE(createTree(sourceDir, expected, 4096), expected);

    int delivered = 0;
    auto *ops = new FileOperationsService();
    QObject::connect(ops, &FileOperationsService::operationFinished, ops,
                     [&](const QString &, const OperationResult &) { ++delivered; });

    ops->copy({sourceDir}, targetDir);
    delete ops; // dies with the copy still running

    const QString copied = targetDir + QDir::separator() + QStringLiteral("tree");
    QTRY_COMPARE_WITH_TIMEOUT(countFiles(copied), expected, 30000);
    QTest::qWait(50); // the delivery had every chance to happen by now
    QCOMPARE(delivered, 0);
}

QTEST_MAIN(FileOpsAsyncTest)
#include "test_file_ops_async.moc"
