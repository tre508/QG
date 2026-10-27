---
type: root-cause-analysis
project: QG
created: 2026-10-21
updated: 2026-10-25
status: resolved
tags:
  - concurrency
  - crash
  - use-after-free
  - worker-pool
  - qg_keyboard
source:
  - FIX-UI-004 (incidental diagnosis)
  - 2026-10-25 worker-delivery work item
  - src/workers/tasks.cpp
  - src/workers/tasks.hpp
  - tests/unit/test_workers.cpp
---

# Root Cause: Worker-Pool Delivery Touched a Destroyed Receiver (use-after-free)

Related: [[Current-State]] · [[Known-Issues]] · [[Threading-Model]] ·
[[2026-10-25-fix-worker-delivery-uaf]] · [[Deferred-Test-Register]]

**Status: RESOLVED 2026-10-25** — mechanism verified, fixed, and validated
(25× repeat runs + ASan/UBSan matrix). Residual same-class sites that were *not*
in this work item's scope are listed at the end.

## Observed Failure (2026-10-21)

`ctest` reported `qg_keyboard` **SEGFAULT** after all 9 of its tests had passed
(the runner prints `Totals: 9 passed` first, then the crash, i.e. teardown).

Earlier project notes recorded this as "environmental, not code-related". That is
**wrong** — it reproduces in a plain sequential loop:

    for i in $(seq 1 25); do ./build/tests/qg_keyboard_test >/dev/null 2>&1 || fails=$((fails+1)); done
    → sequential non-zero exits: 1/25        (≈4 %)
    → under gdb: SIGSEGV on attempt 14 of 20 (≈5-7 %)

## Captured Frame (2026-10-21)

    Thread 11 "Thread (pooled)" received signal SIGSEGV, Segmentation fault.
    0x00007ffff23d95d8 in QObject::thread() const () from libQt6Core.so.6

The crash was on a **QThreadPool worker**, inside `QObject::thread()` — the first
call `QMetaObject::invokeMethod(contextObject, …)` makes on its context object.

## Mechanism (Verified 2026-10-25)

`submitTask()` captured the receiver as a raw pointer and, from the pool thread,
called `QMetaObject::invokeMethod(receiver, …, Qt::QueuedConnection)` to queue the
result. That overload must read the context object's **thread affinity**, so it
dereferences the receiver *from the worker thread*.

Qt's documented behaviour ("a queued event is dropped if the receiver is
destroyed") only applies when the receiver is **still valid at the moment of
posting**. It never covered the window between submission and delivery. When the
receiver is destroyed inside that window there are two distinct failure modes:

1. The worker reads freed memory to find the affinity → SEGV in
   `QObject::thread()` on the pool thread (the 2026-10-21 frame).
2. The post "succeeds" against reused/freed memory, so a `QMetaCallEvent`
   addressed to a dead `QObject` sits in the GUI thread's queue and crashes when
   it is dispatched → SEGV in `QCoreApplicationPrivate::notify_helper()`.

The original code comment ("using `receiver` as the context object makes Qt drop
the event if the receiver is destroyed first") described guarantee (2) but not
the dereference it makes *before* the event exists. The receiver pointer served
two purposes at once — destination-thread selection and lifetime tracking — and
only the second one had no independent owner.

## Deterministic Reproduction (2026-10-25)

`WorkersTest::deliveryDroppedWhenReceiverDestroyed` destroys the receiver while
its task is still in flight (the task blocks until the test deletes the receiver),
which lands squarely in the window above. Built rail-to-rail with
`-DQG_SANITIZE=address`:

| Build | Result |
|---|---|
| **pre-fix** `src/workers/tasks.cpp` (HEAD) + the new test | SEGV **3/3 runs**, exit 1 |
| **post-fix** | 0 crashes in 75 ASan runs (see below) |

Pre-fix ASan report (identical every run):

    ==38021==ERROR: AddressSanitizer: SEGV on unknown address 0x0000000004bb
    ==38021==The signal is caused by a READ memory access.
        #0 QCoreApplicationPrivate::notify_helper(QObject*, QEvent*)
        #1 QCoreApplication::notifyInternal2(QObject*, QEvent*)
        #2 QCoreApplicationPrivate::sendPostedEvents(QObject*, int, QThreadData*)
        ...
        #10 qg::PriorityPool::waitForDone(int)  priority_pool.cpp:155
        #11 WorkersTest::deliveryDroppedWhenReceiverDestroyed()  test_workers.cpp:246

i.e. failure mode (2): a queued call addressed to the destroyed receiver was
dispatched on the GUI thread. Mode (1) is the same defect with different timing;
the historical `QObject::thread()` frame is what the ~4 % teardown flake showed.

## Fix Implemented (2026-10-25)

| File | Change |
|------|--------|
| `src/workers/tasks.cpp` | `deliveryHub()`: one process-lifetime `QObject` created on the GUI thread. `postToGui(guard, fn)`: the guarded GUI-thread hop. `submitTask()`: builds `QPointer<QObject> guarded(receiver)` **on the caller's (GUI) thread**, records `hasReceiver = receiver != nullptr` separately, and never mentions `receiver` again off that thread. |
| `src/workers/tasks.hpp` | Full thread contract (calling thread, `work` thread, `deliver` thread, null-receiver behaviour, receiver-death behaviour, cancellation, affinity assumptions) + `postToGui()` declaration. |
| `src/search/recursive_search_service.{hpp,cpp}` | Call-site fix of the same class: the I/O-pool walk posted batches with `QMetaObject::invokeMethod(this, …)` from the worker thread. It now posts through `postToGui()` with a guard built in `start()`, and uses a by-value token copy instead of the `m_token` member (which also removes a cross-thread race on that `shared_ptr`). |
| `tests/unit/test_workers.cpp` | 6 new regression tests (17 total): receiver destroyed after work finished but before the queued delivery, several receivers destroyed with tasks in flight, null receiver **and** null token, `postToGui` on the GUI thread / dropped when the owner died / dropped for an empty guard. |

Design rules the implementation holds to:

* The worker never dereferences the original receiver — it only copies/queries the
  `QPointer` guard, which is atomic-reference-count-only (see below).
* The guard is created on the GUI thread *before* submission (constructing a
  `QPointer` from a raw pointer is the only dereferencing operation).
* Delivery is queued to a stable GUI-thread dispatcher and the guard is
  **re-checked on the GUI thread** immediately before `deliver()` runs, so a
  receiver destroyed while the result was in flight is discarded.
* `receiver == nullptr` keeps its documented worker-thread delivery (an explicit
  `hasReceiver` flag, because a null guard cannot distinguish "never had a
  receiver" from "the receiver died"; a token cannot stand in for it either,
  since a fire-and-forget task may legitimately carry one).
* Cancellation semantics are unchanged (checked before `work`, after `work`;
  not re-checked once queued — `deliver` is the callers' bookkeeping point and
  callers re-validate their own token/generation).

### Dispatcher lifecycle — why a never-freed object

| Option | Why rejected |
|---|---|
| `QMetaObject::invokeMethod(qApp, …)` | `qApp` is destroyed at the end of `main`; `WorkerPools::io()/cpuMedia()` are function-local statics whose pool threads are only drained *after* `main` returns, so a worker can still post at that point. |
| A function-local `static QObject` hub | Same shutdown window, worse ordering: the hub static is constructed at the first `submitTask()`, i.e. *after* the pool statics, so it would be destroyed *before* them. |
| Destroy the hub after draining the pools | Needs a hook that runs after the pool statics; `PriorityPool::waitForDone(-1)` spins the owning thread's event loop, which no longer exists at that point → a new exit hang. |
| **One heap-allocated `QObject`, allocated once, never freed** (chosen) | It cannot dangle, so no worker can ever hold a stale hub pointer, in any teardown order. Cost: one `QObject` for the process lifetime. LSan does **not** report it: the pointer lives in static storage, so the allocation is reachable (verified with a 6-line probe — reachable allocations are not leaks). No suppression directive is used. |

### Qt guarantees verified against the shipped headers (6.10.2)

Read from `QtCore/qpointer.h` and `QtCore/qsharedpointer_impl.h`:

* `QPointer<T>` wraps `QWeakPointer<QObject>`; its copy constructor is
  compiler-generated → `QWeakPointer(const QWeakPointer &o) : d(o.d), value(o.value)
  { if (d) d->weakref.ref(); }` where `ExternalRefCountData::weakref/strongref` are
  `QBasicAtomicInt`. **Copying is an atomic bump and never touches the object.**
* `isNull()` = `d == nullptr || d->strongref.loadRelaxed() == 0 || value == nullptr`
  — an atomic load plus pointers, **no dereference** of the `QObject`.
* Constructing a `QPointer` from a raw pointer (`QPointer(T *p) : wp(p, true)`)
  *does* read the object (`QObjectPrivate`), hence GUI-thread-only.

So the two operations a pool thread performs on the guard (copy it into the queued
callable, query `isNull()`) are safe; the third (construction) is confined to the
submitting thread. TSan cannot confirm this by itself — libQt6Core is not
instrumented, so TSan cannot see its atomics (see "Sanitizer notes").

## Validation (executed 2026-10-25, minimal build; see [[2026-10-25-fix-worker-delivery-uaf]])

| Check | Command | Result |
|---|---|---|
| Full suite (pre-final-comment) | `ctest --test-dir build-work -j 4` | **46/48 pass**; only pre-existing `qg_qml` (4 documented subtests) and `qg_watcher` |
| Full suite (final tree) | `ctest --test-dir build-work -j 4` | **47/48 pass**; only pre-existing `qg_qml` (4 documented subtests) |
| `qg_watcher` triage | 25× standalone with the **pre-change** binary in `build/` | fails identically (25/25) → flaky on this host, **not** attributable to this change; it exercises folder diff/enumeration and contains no `submitTask` call |
| 25× repeats | `qg_workers`, `qg_keyboard`, `qg_thumb_delivery`, `qg_browser_controller`, `qg_probe_wiring`, `qg_thumb_wiring`, `qg_mounts`, `qg_composition` | **200/200 runs, exit 0, 0 signal deaths** (historical `qg_keyboard` teardown rate was ~1/25) |
| ASan+UBSan 25× | workers/keyboard/thumb_delivery + browser_controller/thumb_wiring | **125/125 runs clean**: 0 SEGV, 0 heap-use-after-free, 0 leak reports, 0 UBSan findings (plus single clean ASan runs of `qg_probe_wiring`, `qg_mounts`, `qg_composition`) |
| Pre-fix control | pre-fix binary under the same ASan flags | SEGV 3/3 → the instrument does catch the defect |
| TSan | separate `-DQG_SANITIZE=thread` build | Delivery tests pass; every report is inside uninstrumented `libQt6Core` (`QWaitCondition::~QWaitCondition`, `QArrayData::reallocateUnaligned`) and **the same reports reproduce in a 15-line Qt-only program with no QG code**; TSan then aborts on its own `sanitizer_thread_registry` CHECK. No usable TSan verdict on this host/Qt combination. |

## Residual same-class sites (recorded, NOT fixed here)

These share the "receiver lifetime vs. cross-thread delivery" shape but live in
other subsystems (render/foreign threads) and need their own work item and tests:

1. `src/playback/video_surface.cpp:99-101` — `onMpvUpdate()` runs on mpv's thread,
   builds a `QPointer<VideoSurfaceItem>` **from raw `userData`** (dereferences the
   item if it is already gone) and then has a TOCTOU window between `if (item)`
   and the queued `invokeMethod(item.data(), "update")`. The pointer arrives from
   mpv, so the fix is on the callback-registration side (clear the update callback
   with the item), not with a worker-side guard.
2. `src/playback/mpv_backend.cpp:84` — `wakeupCallback()` posts to `self` from
   mpv's thread; safe only if no wakeup can be in flight while `MpvBackend` is
   being destroyed.
3. `src/core/events.cpp:23` — `EventBus::post()` queues on `this` from any thread;
   same shape, but currently unreachable (no production callers).
4. `src/workers/priority_pool.cpp:143` — `schedulePump()` queues on `this` from a
   pool thread. Safe for the two static pools; a stack-allocated pool relies on
   Qt removing pending events in `QObject::~QObject`. Unchanged by this work item.

## Lessons

1. "Qt drops queued events for a destroyed receiver" is a guarantee about a
   receiver that is *still alive when the event is posted*. It is not a lifetime
   guard for the posting thread, and the dereference happens before the event
   exists.
2. Never let one pointer carry both "where to deliver" and "is the destination
   alive": give the lifetime question its own owner.
3. A null lifetime guard is ambiguous (never had an owner vs. owner died). Record
   the caller's intent separately instead of inferring it from the guard or from
   an unrelated token.
4. Verify "do not assume X is thread-safe" claims against the shipped headers
   rather than folklore; here the header showed exactly which operation is safe
   off-thread and which is not.
