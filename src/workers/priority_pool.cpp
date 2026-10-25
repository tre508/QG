#include "priority_pool.hpp"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QMetaObject>
#include <QMutexLocker>
#include <QRunnable>

#include <algorithm>

namespace qg {

// Owns the user-supplied QRunnable. If the wrapper is destroyed without having
// run (pool clear/shutdown), the destructor still deletes the inner task.
class PriorityPool::ScheduledTask : public QRunnable {
public:
    ScheduledTask(PriorityPool *pool, QRunnable *task)
        : m_pool(pool), m_task(task)
    {
        setAutoDelete(true);
    }

    ~ScheduledTask() override
    {
        delete m_task;
        m_task = nullptr;
    }

    void run() override
    {
        QRunnable *task = m_task;
        m_task = nullptr;
        if (task != nullptr) {
            task->run();
            delete task;
        }
        m_pool->onTaskFinished();
    }

private:
    PriorityPool *m_pool;
    QRunnable *m_task;
};

PriorityPool::PriorityPool(int maxThreadCount, QObject *parent)
    : QObject(parent)
{
    m_pool.setMaxThreadCount(std::max(1, maxThreadCount));
    // Expire idle threads promptly; pools are long-lived but bursty.
    m_pool.setExpiryTimeout(10000);
}

PriorityPool::~PriorityPool()
{
    {
        QMutexLocker locker(&m_mutex);
        m_shuttingDown = true;
    }
    clear();
    m_pool.waitForDone();
}

void PriorityPool::start(QRunnable *task, TaskPriority priority)
{
    if (task == nullptr) {
        return;
    }

    auto *scheduled = new ScheduledTask(this, task);
    {
        QMutexLocker locker(&m_mutex);
        if (m_shuttingDown) {
            locker.unlock();
            delete scheduled; // deletes `task` too
            return;
        }
        const auto index = static_cast<std::size_t>(priority);
        m_pending[index].push_back(scheduled);
    }
    pump();
}

int PriorityPool::pendingCount() const
{
    QMutexLocker locker(&m_mutex);
    int total = 0;
    for (const auto &queue : m_pending) {
        total += static_cast<int>(queue.size());
    }
    return total;
}

void PriorityPool::clear()
{
    QMutexLocker locker(&m_mutex);
    for (auto &queue : m_pending) {
        for (QRunnable *task : queue) {
            delete task;
        }
        queue.clear();
    }
}

void PriorityPool::pump()
{
    QMutexLocker locker(&m_mutex);
    if (m_shuttingDown) {
        return;
    }

    while (m_running < m_pool.maxThreadCount()) {
        QRunnable *next = nullptr;
        for (auto &queue : m_pending) {
            if (!queue.empty()) {
                next = queue.front();
                queue.pop_front();
                break;
            }
        }
        if (next == nullptr) {
            break;
        }
        ++m_running;
        locker.unlock();
        m_pool.start(next);
        locker.relock();
    }
}

void PriorityPool::onTaskFinished()
{
    {
        QMutexLocker locker(&m_mutex);
        if (m_running > 0) {
            --m_running;
        }
    }
    schedulePump();
}

void PriorityPool::schedulePump()
{
    QMetaObject::invokeMethod(
        this, [this]() { pump(); }, Qt::QueuedConnection);
}

void PriorityPool::waitForDone(int msec)
{
    QElapsedTimer timer;
    timer.start();

    for (;;) {
        pump();
        m_pool.waitForDone(5);
        QCoreApplication::processEvents(QEventLoop::AllEvents, 5);

        {
            QMutexLocker locker(&m_mutex);
            if (m_running == 0) {
                bool empty = true;
                for (const auto &queue : m_pending) {
                    if (!queue.empty()) {
                        empty = false;
                        break;
                    }
                }
                if (empty) {
                    return;
                }
            }
        }

        if (msec >= 0 && timer.elapsed() > msec) {
            return;
        }
    }
}

} // namespace qg