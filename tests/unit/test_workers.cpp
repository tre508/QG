// Worker/task infrastructure tests (Prompt 04).
//
// Covers: monotonic token generations, stale-generation detection, cooperative
// cancellation at chunk boundaries, bounded pool concurrency, priority ordering
// and queued GUI-thread delivery.

#include <QtTest/QtTest>

#include <QList>
#include <QMutex>
#include <QMutexLocker>
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

QTEST_GUILESS_MAIN(WorkersTest)
#include "test_workers.moc"