#include "tasks.hpp"

#include <QMetaObject>
#include <QThread>

#include <algorithm>
#include <utility>

namespace qg {

FunctionRunnable::FunctionRunnable(std::function<void()> fn)
    : m_fn(std::move(fn))
{
    setAutoDelete(true);
}

void FunctionRunnable::run()
{
    if (m_fn) {
        m_fn();
    }
}

namespace {

int cpuCount()
{
    return std::max(1, QThread::idealThreadCount());
}

} // namespace

PriorityPool &WorkerPools::io()
{
    static PriorityPool pool(std::min(4, cpuCount()));
    return pool;
}

PriorityPool &WorkerPools::cpuMedia()
{
    static PriorityPool pool(std::min(cpuCount(), 6));
    return pool;
}

int WorkerPools::ioThreadCount()
{
    return io().maxThreadCount();
}

int WorkerPools::cpuMediaThreadCount()
{
    return cpuMedia().maxThreadCount();
}

void submitTask(PriorityPool &pool,
                TaskPriority priority,
                std::shared_ptr<TaskToken> token,
                QObject *receiver,
                std::function<void()> work,
                std::function<void()> deliver)
{
    auto *runnable = new FunctionRunnable(
        [token, receiver, work = std::move(work), deliver = std::move(deliver)]() mutable {
            if (token && token->isCancelled()) {
                return;
            }
            if (work) {
                work();
            }
            if (token && token->isCancelled()) {
                return;
            }
            if (receiver == nullptr) {
                if (deliver) {
                    deliver();
                }
                return;
            }
            // Deliver on the receiver's thread. Using `receiver` as the context
            // object makes Qt drop the event if the receiver is destroyed first.
            QMetaObject::invokeMethod(
                receiver,
                [deliver = std::move(deliver)]() mutable {
                    if (deliver) {
                        deliver();
                    }
                },
                Qt::QueuedConnection);
        });
    pool.start(runnable, priority);
}

} // namespace qg