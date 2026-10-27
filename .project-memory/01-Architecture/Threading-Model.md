---
type: architecture
project: QG
created: 2026-10-20
updated: 2026-10-20
status: verified
tags:
  - architecture
  - threading
  - concurrency
  - tasktoken
source:
  - DESIGN.md
  - ARCHITECTURE_CURRENT.md
  - src/workers/
  - src/controllers/
---

# QG Threading Model

## Two Bounded QThreadPools

| Pool | Threads | Priority Levels | Work Types |
|------|---------|-----------------|------------|
| **I/O Pool** | `min(4, cpu_count)` | Interactive, Normal, Background | Enumeration, file ops (copy/move/trash), mount discovery, directory watching |
| **CPU/Media Pool** | `min(cpu_count, 6)` | Interactive, Normal, Background | Thumbnail decode/scaling, media probing (FFmpeg), hashing |

## TaskToken & Cancellation

```cpp
// src/workers/task_token.hpp
class TaskToken {
    std::uint64_t generation_;      // Monotonic counter
    std::string path_or_identity_;  // Work scope
    std::atomic<bool> cancelled_;   // Cooperative cancellation
};
```

**Mechanics:**
1. Each navigation/search/sort operation increments global generation counter
2. `TaskToken` captures generation at submission time
3. Workers check `token->isCancelled()` at chunk boundaries
4. On completion, controllers validate: `token == current_token && !token->isCancelled()`
5. Stale results dropped before model mutation

## Thread Boundaries

```
┌─────────────────────────────────────────────────────────────────┐
│ GUI THREAD (Qt Main)                                            │
│  QML Engine, Controllers, Models, QML Bindings                 │
│  • Models mutated ONLY here (via queued signals)               │
│  • QML never accessed from workers                             │
└──────────────────────────┬──────────────────────────────────────┘
                           │ Queued Signals / invokeMethod
                           ▼
┌─────────────────────────────────────────────────────────────────┐
│ I/O POOL (min(4, cpu))                                          │
│  FilesystemEnumerator, FileOperationsService, TrashService,     │
│  MountService (polling), DirectoryWatcher callbacks             │
│  • TaskToken checked at batch boundaries                        │
│  • Results delivered via QMetaObject::invokeMethod(Qt::Queued) │
└──────────────────────────┬──────────────────────────────────────┘
                           │ Queued Signals
                           ▼
┌─────────────────────────────────────────────────────────────────┐
│ CPU/MEDIA POOL (min(cpu, 6))                                    │
│  ThumbnailService (FFmpeg decode/scale), MediaProbeService      │
│  • ≤2 concurrent probes (Background priority)                   │
│  • Visible-first thumbnail scheduling                          │
│  • Results: QImage only, never GPU resources                   │
└──────────────────────────┬──────────────────────────────────────┘
                           │ mpv wakeup callback
                           ▼
┌─────────────────────────────────────────────────────────────────┐
│ MPV INTERNAL THREADS                                            │
│  Decode, filter, audio — owned by libmpv                       │
│  • wakeupCallback posts MpvIdleEvent to Qt thread              │
│  • Property observation via mpv_observe_property               │
└──────────────────────────┬──────────────────────────────────────┘
                           │ Render Thread (Qt Scene Graph)
                           ▼
┌─────────────────────────────────────────────────────────────────┐
│ RENDER THREAD (Qt Quick Scene Graph)                            │
│  VideoRenderNode (QSGRenderNode)                                │
│  • Calls mpv_render_context_render() into Qt FBO               │
│  • Workers return QImage only, never GPU resources             │
└─────────────────────────────────────────────────────────────────┘
```

## Priority System

```cpp
enum class TaskPriority { Interactive, Normal, Background };
```

- **Interactive**: First screenful enumeration, visible thumbnails, user-initiated actions
- **Normal**: Batch enumeration continuation, adjacent thumbnails, probes
- **Background**: Bulk operations, deep thumbnail generation, recursive search

## Cancellation Scenarios

| Trigger | Action |
|---------|--------|
| Navigate to new folder | `BrowserController` advances generation; old enumeration cancelled |
| Search query change | `SearchController` advances generation; old search cancelled |
| Sort order change | `BrowserController` calls `model->reorder()`; no worker cancel needed |
| Window close | `App` destructor stops `MountService`, waits for pools |

## Verified Thread Safety

- ✅ **No model mutations from workers** — all via queued callbacks to GUI thread
- ✅ **No QML access from workers** — workers return `QImage`/`QString`/`FolderSnapshot` only
- ✅ **No GPU resources cross threads** — `VideoRenderNode` owns GL; workers return CPU images
- ✅ **TaskToken validated at every completion** — stale results rejected
- ✅ **libmpv events marshaled** — `wakeupCallback` → `MpvIdleEvent` → Qt event loop
- ✅ **SQLite per-thread connections** — `Database` object per thread, WAL mode

## Worker Pool Configuration

```cpp
// src/workers/priority_pool.cpp
PriorityPool::PriorityPool(int maxThreads)
    : m_pool(maxThreads)  // QThreadPool with reserved threads per priority
```

- `WorkerPools::io()` — static accessor for the I/O pool
- `WorkerPools::cpuMedia()` — static accessor for the CPU/Media pool
- `submitTask(pool, priority, token, receiver, work, deliver)` — unified submission
- `postToGui(guard, fn)` — guarded GUI-thread hop, used directly by worker code
  that produces incremental results (e.g. `RecursiveSearchService::walk`)

### Result delivery is receiver-lifetime-safe

`submitTask()` must be called from the GUI thread. It builds a `QPointer<QObject>`
guard for `receiver` **there**, and records the null-receiver case separately;
pool threads only copy/query the guard (atomic reference counts — never a
dereference). Results are queued through one process-lifetime GUI-thread hub and
the guard is re-checked on the GUI thread immediately before `deliver()` runs, so
a receiver destroyed while a result was in flight is discarded instead of being
delivered. `work` never touches GUI state or the receiver; a null `receiver` still
means "deliver on the worker thread". `receiver` is a lifetime token only — it
does **not** select the delivery thread. See
[[worker-pool-receiver-use-after-free]].

### File operations are an asynchronous facade (AUD-003, 2026-10-25)

`FileOperationsService`'s mutating `Q_INVOKABLE`s (`rename`, `move`, `copy`,
`deletePermanently`) do **not** do I/O on the calling thread. Each one submits
its `*Sync` helper to `WorkerPools::io()` at `TaskPriority::Normal` through
`submitTask(…, this, work, deliver)`, so:

- `work` runs the whole directory walk / copy / delete on a pool thread and never
  touches GUI state;
- `deliver` (`emitFinished` → `operationFinished`) runs on the GUI thread, after
  the event loop turns, and only while the service is still alive;
- the `Q_INVOKABLE` itself returns in microseconds — measured pre-fix: **818 ms**
  of GUI-thread block for a 3 MB / 400-file copy (`qg_file_ops_async`).

Consequence for callers: a result is observed *after* the event loop turns, in
**completion** order (not request order), so per-request state must be correlated
with the result it belongs to. `ActionsController`/QML already decide before
dispatching (`targetPathForRename`/`pathExists`), so they needed no change.

`reveal`/`openWith` still launch synchronously on the GUI thread (blocking
`QProcess`), and `TrashService` is still called inline by
`ActionsController::trash` — both out of AUD-003's scope. Progress/cancellation
is FIX-FO-02. See [[2026-10-25-fix-aud-003-file-ops-async]].

## Performance Characteristics

| Operation | Pool | Typical Latency | Cancellation |
|-----------|------|-----------------|--------------|
| First screenful (10k entries) | I/O | ~300ms | At batch boundaries (~200 entries) |
| Thumbnail extraction (1080p) | CPU/Media | ~50-100ms | After frame decode |
| Media probe (duration) | CPU/Media | ~10-50ms | After format open |
| Recursive search (100k files) | I/O | ~2-5s | At directory boundaries |
| Move 1000 files | I/O | ~1-2s | None yet (FIX-FO-02) |
| Directory copy (3 MB / 400 files) | I/O | ~0.8s | None yet (FIX-FO-02) |