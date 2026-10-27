// Worker/task infrastructure tests (Prompt 04).
//
// Covers: monotonic token generations, stale-generation detection, cooperative
// cancellation at chunk boundaries, bounded pool concurrency, priority ordering
// and queued GUI-thread delivery.

#include <QtTest/QtTest>

#include <QCoreApplication>
#include <QList>
#include <QMutex>
#include <QMutexLocker>
#include <QPointer>
#include <QThread>

#include <atomic>
#include <memory>

#include "workers/priority_pool.hpp"
#include "workers/task_token.hpp"
#include "workers/tasks.hpp"

using namespace qg;

class WorkersTest : public QObject
{
    Q_OBJECT

private slots:
    void tokenGenerationIsMonotonic();
    void staleGenerationDetection();
    void cancellationObservedAtChunkBoundaries();
    void poolConcurrencyCapRespected();
    void interactivePriorityPrecedesBackground();
    void queuedDeliveryRunsOnReceiverThread();
    void cancelledTaskDoesNotDeliver();
    void fireAndForgetDeliversOnWorkerThread();
    void deliveryDroppedWhenReceiverDestroyed();
    void deliveryDroppedWhenReceiverDestroyedAfterWork();
    void deliveryDroppedForMultipleQueuedTasks();
    void nullReceiverWithNullTokenDeliversOnWorkerThread();
    void postToGuiRunsOnGuiThread();
    void postToGuiDroppedWhenOwnerDestroyed();
    void postToGuiDroppedForEmptyGuard();
};

void WorkersTest::tokenGenerationIsMonotonic()
{
    TaskToken first;
    TaskToken second;
    TaskToken third;
    QVERIFY(second.generation() > first.generation());
    QVERIFY(third.generation() > second.generation());
    QVERIFY(first.generation() >= 1);
}

void WorkersTest::staleGenerationDetection()
{
    TaskToken token;
    QVERIFY(!token.isStale(token.generation()));
    QVERIFY(token.isStale(token.generation() + 1));

    TaskToken other;
    QVERIFY(token.isStale(other.generation()));
}

void WorkersTest::cancellationObservedAtChunkBoundaries()
{
    auto token = std::make_shared<TaskToken>();
    auto chunks = std::make_shared<std::atomic<int>>(0);

    auto *runnable = new FunctionRunnable([token, chunks]() {
        for (int i = 0; i < 200; ++i) {
            if (token->isCancelled()) {
                break;
            }
            QThread::msleep(2);
            chunks->fetch_add(1);
        }
    });
    WorkerPools::cpuMedia().start(runnable, TaskPriority::Normal);

    QTRY_VERIFY_WITH_TIMEOUT(chunks->load() >= 5, 3000);

    token->cancel();
    WorkerPools::cpuMedia().waitForDone(5000);

    const int afterCancel = chunks->load();
    QTest::qWait(50);
    QCOMPARE(chunks->load(), afterCancel); // no progress after cancellation
    QVERIFY(afterCancel < 200);
    QVERIFY(token->isCancelled());
}

void WorkersTest::poolConcurrencyCapRespected()
{
    PriorityPool pool(2);

    std::atomic<int> active{0};
    std::atomic<int> maxActive{0};
    std::atomic<int> completed{0};

    for (int i = 0; i < 8; ++i) {
        pool.start(new FunctionRunnable([&active, &maxActive, &completed]() {
            const int now = active.fetch_add(1) + 1;
            int prev = maxActive.load();
            while (now > prev && !maxActive.compare_exchange_weak(prev, now)) {
            }
            QThread::msleep(30);
            active.fetch_sub(1);
            completed.fetch_add(1);
        }));
    }

    pool.waitForDone(10000);

    QCOMPARE(completed.load(), 8);
    QVERIFY(maxActive.load() <= 2);
}

void WorkersTest::interactivePriorityPrecedesBackground()
{
    PriorityPool pool(1);

    QMutex orderMutex;
    QList<QString> order;
    std::atomic<bool> holding{true};
    std::atomic<bool> started{false};

    // Occupy the single worker so the following tasks must queue.
    pool.start(new FunctionRunnable([&holding, &started]() {
        started.store(true);
        while (holding.load()) {
            QThread::msleep(1);
        }
    }));

    QTRY_VERIFY_WITH_TIMEOUT(started.load(), 3000);

    pool.start(new FunctionRunnable([&]() {
        QMutexLocker locker(&orderMutex);
        order.append(QStringLiteral("background"));
    }), TaskPriority::Background);

    pool.start(new FunctionRunnable([&]() {
        QMutexLocker locker(&orderMutex);
        order.append(QStringLiteral("interactive"));
    }), TaskPriority::Interactive);

    holding.store(false);
    pool.waitForDone(10000);

    QMutexLocker locker(&orderMutex);
    QCOMPARE(order.size(), 2);
    QCOMPARE(order.at(0), QStringLiteral("interactive"));
    QCOMPARE(order.at(1), QStringLiteral("background"));
}

void WorkersTest::queuedDeliveryRunsOnReceiverThread()
{
    QThread *const mainThread = QThread::currentThread();
    auto token = std::make_shared<TaskToken>();

    QObject receiver;
    std::atomic<bool> workRan{false};
    std::atomic<bool> delivered{false};
    std::atomic<QThread *> deliverThread{nullptr};

    submitTask(WorkerPools::cpuMedia(),
               TaskPriority::Normal,
               token,
               &receiver,
               [&workRan]() { workRan.store(true); },
               [&delivered, &deliverThread]() {
                   delivered.store(true);
                   deliverThread.store(QThread::currentThread());
               });

    QTRY_VERIFY_WITH_TIMEOUT(workRan.load(), 3000);
    QTRY_VERIFY_WITH_TIMEOUT(delivered.load(), 3000);
    QCOMPARE(deliverThread.load(), mainThread);
}

void WorkersTest::cancelledTaskDoesNotDeliver()
{
    auto token = std::make_shared<TaskToken>();
    token->cancel();

    QObject receiver;
    std::atomic<bool> delivered{false};

    submitTask(WorkerPools::cpuMedia(),
               TaskPriority::Normal,
               token,
               &receiver,
               []() {},
               [&delivered]() { delivered.store(true); });

    QTest::qWait(200);
    QVERIFY(!delivered.load());
}

// A null receiver means "fire and forget": no GUI affinity was asked for, so
// the callback runs on the worker thread rather than being dropped. This pins
// the behaviour that a cancellation token must not suppress delivery.
void WorkersTest::fireAndForgetDeliversOnWorkerThread()
{
    auto token = std::make_shared<TaskToken>();
    std::atomic<bool> delivered{false};
    std::atomic<QThread *> deliverThread{nullptr};

    submitTask(WorkerPools::cpuMedia(),
               TaskPriority::Normal,
               token,
               nullptr,
               []() {},
               [&delivered, &deliverThread]() {
                   delivered.store(true);
                   deliverThread.store(QThread::currentThread());
               });

    QTRY_VERIFY_WITH_TIMEOUT(delivered.load(), 3000);
    QVERIFY(deliverThread.load() != QThread::currentThread());
}

// Regression: destroying the receiver while its task is still running must not
// leave a dangling pointer for the worker to hand to invokeMethod() (the
// previous use-after-free). The result is dropped instead of delivered.
void WorkersTest::deliveryDroppedWhenReceiverDestroyed()
{
    auto *receiver = new QObject();
    std::atomic<bool> workStarted{false};
    std::atomic<bool> release{false};
    std::atomic<bool> delivered{false};

    submitTask(WorkerPools::cpuMedia(),
               TaskPriority::Normal,
               nullptr,
               receiver,
               [&workStarted, &release]() {
                   workStarted.store(true);
                   while (!release.load()) {
                       QThread::msleep(1);
                   }
               },
               [&delivered]() { delivered.store(true); });

    QTRY_VERIFY_WITH_TIMEOUT(workStarted.load(), 3000);

    delete receiver; // GUI-side destruction mid-flight
    receiver = nullptr;
    release.store(true);

    WorkerPools::cpuMedia().waitForDone(5000);
    QTest::qWait(200); // give any (wrongly) queued delivery a chance to run

    QVERIFY(!delivered.load());
}

// Regression: the result has already been queued when the receiver dies. The
// guard has to be re-checked on the GUI thread, so the doomed result is
// discarded instead of being delivered to a destroyed receiver.
void WorkersTest::deliveryDroppedWhenReceiverDestroyedAfterWork()
{
    auto *receiver = new QObject();
    std::atomic<bool> workDone{false};
    std::atomic<bool> delivered{false};

    submitTask(WorkerPools::cpuMedia(),
               TaskPriority::Normal,
               nullptr,
               receiver,
               [&workDone]() { workDone.store(true); },
               [&delivered]() { delivered.store(true); });

    // Spin without running the event loop, so the queued delivery stays pending
    // while the receiver disappears.
    while (!workDone.load()) {
        QThread::msleep(1);
    }
    QThread::msleep(50); // let the worker queue the (now doomed) delivery

    delete receiver;
    receiver = nullptr;

    QCoreApplication::processEvents();
    QTest::qWait(50);

    QVERIFY(!delivered.load());
}

// Regression: several receivers, each with a task in flight, all destroyed at
// once. Every result must be dropped and no worker may touch freed memory.
void WorkersTest::deliveryDroppedForMultipleQueuedTasks()
{
    constexpr int kTasks = 3;
    PriorityPool pool(kTasks); // one worker per task, so all three are in flight

    std::atomic<bool> release{false};
    std::atomic<int> started{0};
    std::atomic<int> delivered{0};
    QList<QObject *> receivers;

    for (int i = 0; i < kTasks; ++i) {
        auto *receiver = new QObject();
        receivers.append(receiver);
        submitTask(pool,
                   TaskPriority::Normal,
                   nullptr,
                   receiver,
                   [&release, &started]() {
                       started.fetch_add(1);
                       while (!release.load()) {
                           QThread::msleep(1);
                       }
                   },
                   [&delivered]() { delivered.fetch_add(1); });
    }

    QTRY_VERIFY_WITH_TIMEOUT(started.load() == kTasks, 3000);

    for (QObject *receiver : receivers) {
        delete receiver;
    }
    receivers.clear();
    release.store(true);

    pool.waitForDone(5000);
    QTest::qWait(100);

    QCOMPARE(delivered.load(), 0);
}

// Null receiver *and* null token: the documented fire-and-forget combination,
// still delivered on the worker thread.
void WorkersTest::nullReceiverWithNullTokenDeliversOnWorkerThread()
{
    std::atomic<bool> delivered{false};
    std::atomic<QThread *> deliverThread{nullptr};

    submitTask(WorkerPools::cpuMedia(),
               TaskPriority::Normal,
               nullptr,
               nullptr,
               []() {},
               [&delivered, &deliverThread]() {
                   delivered.store(true);
                   deliverThread.store(QThread::currentThread());
               });

    QTRY_VERIFY_WITH_TIMEOUT(delivered.load(), 3000);
    QVERIFY(deliverThread.load() != QThread::currentThread());
}

// postToGui() is the primitive worker code uses to hand results back without
// touching a GUI-owned object: it must always run the payload on the GUI thread.
void WorkersTest::postToGuiRunsOnGuiThread()
{
    QThread *const guiThread = QThread::currentThread();
    QObject owner;
    const QPointer<QObject> guard(&owner);

    std::atomic<bool> ran{false};
    std::atomic<QThread *> ranOn{nullptr};

    WorkerPools::cpuMedia().start(new FunctionRunnable([guard, &ran, &ranOn]() {
        // Posted from a pool thread, like a real worker result.
        postToGui(guard, [&ran, &ranOn]() {
            ran.store(true);
            ranOn.store(QThread::currentThread());
        });
    }));
    WorkerPools::cpuMedia().waitForDone(5000);

    QTRY_VERIFY_WITH_TIMEOUT(ran.load(), 3000);
    QCOMPARE(ranOn.load(), guiThread);
}

void WorkersTest::postToGuiDroppedWhenOwnerDestroyed()
{
    auto *owner = new QObject();
    const QPointer<QObject> guard(owner);
    bool ran = false;

    delete owner; // the guard is all that is left of the owner

    postToGui(guard, [&ran]() { ran = true; });
    QTest::qWait(50);

    QVERIFY(guard.isNull());
    QVERIFY(!ran);
}

// An empty guard is ambiguous exactly like a null receiver — "never had an
// owner" and "the owner died" look the same — so the payload is dropped rather
// than run unguarded. Running it would hand `fn` (which normally captures
// `this`) to a possibly-destroyed owner, i.e. the very hazard postToGui()
// exists to remove.
void WorkersTest::postToGuiDroppedForEmptyGuard()
{
    bool ran = false;

    postToGui(QPointer<QObject>(), [&ran]() { ran = true; });
    QTest::qWait(100);

    QVERIFY(!ran);
}

QTEST_GUILESS_MAIN(WorkersTest)
#include "test_workers.moc"