#include "tasks.hpp"

#include <QCoreApplication>
#include <QMetaObject>
#include <QObject>
#include <QPointer>
#include <QString>
#include <QThread>

#include <algorithm>
#include <mutex>
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

// Stable GUI-thread delivery target for worker results.
//
// Queueing the payload *on the receiver itself* makes the worker read the
// receiver's thread affinity, i.e. dereference the receiver, from a pool
// thread. That is the use-after-free this hub removes: when the receiver was
// destroyed between submission and delivery, the worker either crashed reading
// freed memory inside QObject::thread(), or posted a queued event addressed to
// freed memory that crashed later on the GUI thread. Qt drops queued events
// only for a receiver that is still valid at that moment; it never protects
// that window.
//
// The hub is a process-lifetime singleton and is deliberately never destroyed:
// one QObject allocated once, so no worker can ever hold a dangling hub pointer,
// whatever order the event loop, the pools and the application's statics tear
// down in. It is not reported as a leak either — the pointer lives in static
// storage, so the allocation stays reachable (verified under ASan+LSan). No
// Q_OBJECT is needed — invokeMethod() wants a context object, not a slot.
QObject *deliveryHub()
{
    static QObject *hub = nullptr;
    static std::once_flag once;

    std::call_once(once, [] {
        hub = new QObject();
        hub->setObjectName(QStringLiteral("qg.deliveryHub"));

        if (QCoreApplication::instance() != nullptr) {
            hub->moveToThread(QCoreApplication::instance()->thread());
        }
    });

    return hub;
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

void postToGui(const QPointer<QObject> &guard, std::function<void()> fn)
{
    if (!fn) {
        return;
    }

    QMetaObject::invokeMethod(
        deliveryHub(),
        [guard, fn = std::move(fn)]() mutable {
            if (guard.isNull()) {
                return; // the owner died while the result was in flight
            }
            fn();
        },
        Qt::QueuedConnection);
}

void submitTask(PriorityPool &pool,
                TaskPriority priority,
                std::shared_ptr<TaskToken> token,
                QObject *receiver,
                std::function<void()> work,
                std::function<void()> deliver)
{
    // "No receiver was given" and "the receiver has since died" are different
    // states that both produce a null guard, so the caller's intent is recorded
    // separately here. A cancellation token cannot stand in for it either: a
    // legitimate fire-and-forget task may carry a token too.
    const bool hasReceiver = receiver != nullptr;

    // Lifetime guard, built on the GUI thread before the task is submitted:
    // constructing a QPointer from a raw pointer reads the object, so it must
    // happen while the receiver is known to be alive. Afterwards the guard is
    // only ever copied (an atomic weakref bump) or queried with isNull() (an
    // atomic strongref load); neither dereferences the receiver, so pool threads
    // may use it freely.
    const QPointer<QObject> guarded(receiver);

    auto *runnable = new FunctionRunnable(
        [token, hasReceiver, guarded, work = std::move(work), deliver = std::move(deliver)]() mutable {
            if (token && token->isCancelled()) {
                return;
            }

            if (work) {
                work();
            }

            if (token && token->isCancelled()) {
                return;
            }

            if (!hasReceiver) {
                // Fire and forget: no GUI affinity was requested, so the
                // callback stays on this worker thread.
                if (deliver) {
                    deliver();
                }
                return;
            }

            if (guarded.isNull()) {
                return; // receiver already gone: the result is worthless
            }

            // Hand the payload to the GUI thread. `receiver` is never mentioned
            // again — that is what makes this safe. postToGui() re-checks the
            // guard on the GUI thread before `deliver` runs.
            postToGui(guarded, std::move(deliver));
        });

    pool.start(runnable, priority);
}

} // namespace qg
