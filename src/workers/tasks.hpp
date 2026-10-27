#pragma once

// Shared background-work substrate (DESIGN.md 15.3, 22.2).
//
// WorkerPools owns the application's only two bounded pools. Features must
// submit work through these rather than creating threads or pools of their own.
//
// submitTask() adapts a worker-safe callable and delivers its result back to a
// GUI-owned receiver, dropping the result if the token was cancelled or the
// receiver is gone by the time the result is ready.

#include "priority_pool.hpp"
#include "task_token.hpp"

#include <QObject>
#include <QPointer>

#include <functional>
#include <memory>

namespace qg {

// A QRunnable that runs a copyable callable exactly once.
class FunctionRunnable : public QRunnable {
public:
    explicit FunctionRunnable(std::function<void()> fn);
    void run() override;

private:
    std::function<void()> m_fn;
};

class WorkerPools {
public:
    // I/O pool: enumeration, move/copy/trash. Bounded to min(4, cpu).
    static PriorityPool &io();

    // CPU/media pool: thumbnail decode, scaling, probing. Bounded to min(cpu, 6).
    static PriorityPool &cpuMedia();

    // Total worker threads currently configured (for diagnostics/tests).
    static int ioThreadCount();
    static int cpuMediaThreadCount();
};

// Run `work` on `pool` at `priority`. When the work is done and `token` (if any)
// has not been cancelled, queue `deliver` onto the GUI thread; `deliver` runs
// only while `receiver` is still alive.
//
// Thread contract
// ---------------
// * submitTask() must be called from the GUI thread (the thread running the
//   application's event loop). That is what makes the receiver's lifetime guard
//   valid: the guard is built here, on this thread.
// * `work` runs on a pool thread. It must not touch GUI state, and it must not
//   touch `receiver` or anything else owned by the GUI thread.
// * `receiver` is only a *lifetime token* — it is never dereferenced off the GUI
//   thread, and it does not select the delivery thread. It must therefore be
//   GUI-affine (constructed and destroyed on the GUI thread); the result is
//   always queued to the GUI thread.
// * `deliver` runs on the GUI thread, and only while `receiver` is alive. The
//   guard is re-checked there, after the queued hop, so a receiver destroyed
//   while the result was in flight makes the result be discarded — the
//   `this`-capturing lambdas callers normally pass are then never run.
// * `receiver == nullptr` means "fire and forget": no GUI affinity was asked
//   for, so `deliver` runs on the worker thread.
// * A null/empty `deliver` is legal and is simply not invoked.
// * `token` may be null, in which case no cancellation check is performed.
//   Cancelled before `work` starts: neither `work` nor `deliver` runs.
//   Cancelled while `work` runs: the result is dropped.
//   Cancellation is not re-checked once the result has been queued: `deliver`
//   is also the caller's bookkeeping point (clearing a "busy" flag, continuing a
//   pump loop), so callers stay responsible for re-validating their own token or
//   generation inside `deliver`.
void submitTask(PriorityPool &pool,
                TaskPriority priority,
                std::shared_ptr<TaskToken> token,
                QObject *receiver,
                std::function<void()> work,
                std::function<void()> deliver);

// Queue `fn` onto the GUI thread, to run only while the object behind `guard`
// is still alive. This is the receive-side counterpart of submitTask(): worker
// code can hand a result back to a GUI-owned object without ever touching that
// object, because a guard is only read, never dereferenced — QPointer copies
// and isNull() are atomic-reference-count operations.
//
// `guard` must have been created on the GUI thread before the work started
// (constructing a QPointer from a raw pointer reads the object). An empty guard
// means "the owner is not known to be alive", so `fn` is dropped — it is never
// run unguarded, because "never had an owner" and "the owner has died" are
// indistinguishable through a QPointer.
//
// Callable from any thread, including pool threads.
void postToGui(const QPointer<QObject> &guard, std::function<void()> fn);

} // namespace qg
