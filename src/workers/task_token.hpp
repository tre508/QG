#pragma once

// TaskToken: the identity + cancellation handle that accompanies every
// long-running background task (DESIGN.md 15.3, 22.5).
//
// A token carries a monotonic *generation*. When a newer generation supersedes
// an older one (e.g. the user navigates away), results tagged with the old
// generation are stale and must be dropped before they can mutate a model.
//
// The token is shared between the GUI thread (which may cancel) and the worker
// thread (which observes cancellation), so the cancellation flag is atomic.
// The path/identity payload is written once before dispatch and read on the
// worker, so it is a plain value.

#include <QString>

#include <atomic>
#include <cstdint>

namespace qg {

class TaskToken {
public:
    using Generation = std::uint64_t;

    // Fresh token with a new, globally increasing generation.
    TaskToken();

    explicit TaskToken(Generation generation);

    TaskToken(const TaskToken &) = delete;
    TaskToken &operator=(const TaskToken &) = delete;
    TaskToken(TaskToken &&) = delete;
    TaskToken &operator=(TaskToken &&) = delete;

    ~TaskToken() = default;

    // Next value from the process-wide monotonic generation counter.
    static Generation nextGeneration() noexcept;

    Generation generation() const noexcept { return m_generation; }

    // Cooperative cancellation. Safe to call from any thread.
    void cancel() noexcept { m_cancelled.store(true, std::memory_order_relaxed); }
    bool isCancelled() const noexcept { return m_cancelled.load(std::memory_order_relaxed); }

    // A result produced under `generation` is stale for this token when it does
    // not match the token's own generation.
    bool isStale(Generation generation) const noexcept { return generation != m_generation; }

    // Optional identity/path context (set before dispatch, read on the worker).
    void setPath(const QString &path) { m_path = path; }
    const QString &path() const noexcept { return m_path; }

    void setIdentity(std::uint64_t identity) noexcept { m_identity = identity; }
    std::uint64_t identity() const noexcept { return m_identity; }

private:
    Generation m_generation;
    std::atomic<bool> m_cancelled{false};
    QString m_path;
    std::uint64_t m_identity = 0;
};

} // namespace qg