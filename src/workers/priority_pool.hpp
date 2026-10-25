#pragma once

// PriorityPool: a bounded QThreadPool with three priority classes
// (DESIGN.md 15.3, 22.2).
//
// Each pool owns exactly one QThreadPool, so concurrency is bounded by
// maxThreadCount(). Tasks are held in per-priority queues and only handed to
// the thread pool while a worker slot is free, so a flood of `background`
// work can never starve `interactive` work.
//
// Threading contract:
//   * start()/clear()/waitForDone() are called from the pool's owning thread
//     (the GUI thread in production, the test thread under QTest).
//   * Worker threads only run the submitted QRunnables; they never touch GUI
//     state. Completion is re-scheduled onto the owning thread via a queued
//     invocation.
//
// start() takes ownership of the submitted QRunnable: it is deleted after it
// runs, or when the pool is cleared/destroyed before it runs.

#include <QMutex>
#include <QObject>
#include <QThreadPool>

#include <array>
#include <deque>

class QRunnable;

namespace qg {

enum class TaskPriority {
    Interactive = 0, // visible items
    Normal = 1,      // viewport-adjacent
    Background = 2,  // prefetch / bulk operations
};

class PriorityPool : public QObject {
    Q_OBJECT

public:
    explicit PriorityPool(int maxThreadCount, QObject *parent = nullptr);
    ~PriorityPool() override;

    PriorityPool(const PriorityPool &) = delete;
    PriorityPool &operator=(const PriorityPool &) = delete;

    // Takes ownership of `task`. Must be called from the owning thread.
    void start(QRunnable *task, TaskPriority priority = TaskPriority::Normal);

    int maxThreadCount() const { return m_pool.maxThreadCount(); }
    int activeThreadCount() const { return m_pool.activeThreadCount(); }

    // Tasks accepted but not yet handed to a worker.
    int pendingCount() const;

    // Drop all pending tasks (running tasks are allowed to finish).
    void clear();

    // Block until all queued tasks have run, spinning the owning thread's event
    // loop so completion callbacks can be processed. `msec < 0` waits forever.
    void waitForDone(int msec = -1);

private:
    class ScheduledTask;

    // Internal: called on the owning thread to hand pending work to workers.
    void pump();

    // Internal: called on a worker thread when a task finishes.
    void onTaskFinished();

    void schedulePump();

    QThreadPool m_pool;
    mutable QMutex m_mutex;
    std::array<std::deque<QRunnable *>, 3> m_pending;
    int m_running = 0;
    bool m_shuttingDown = false;
};

} // namespace qg