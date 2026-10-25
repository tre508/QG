#pragma once

// Shared background-work substrate (DESIGN.md 15.3, 22.2).
//
// WorkerPools owns the application's only two bounded pools. Features must
// submit work through these rather than creating threads or pools of their own.
//
// submitTask() adapts a worker-safe callable and delivers its result back to a
// GUI-owned receiver through a queued invocation, dropping the result if the
// token was cancelled in the meantime.

#include "priority_pool.hpp"
#include "task_token.hpp"

#include <QObject>

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

// Run `work` on `pool` at `priority`. On completion, if `token` has not been
// cancelled, invoke `deliver` on `receiver`'s thread through a queued
// connection. `work` must not touch GUI state; `deliver` runs on the GUI thread.
// The token may be null, in which case no cancellation check is performed.
void submitTask(PriorityPool &pool,
                TaskPriority priority,
                std::shared_ptr<TaskToken> token,
                QObject *receiver,
                std::function<void()> work,
                std::function<void()> deliver);

} // namespace qg