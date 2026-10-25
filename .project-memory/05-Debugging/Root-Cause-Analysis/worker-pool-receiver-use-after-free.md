---
type: root-cause-analysis
project: QG
created: 2026-10-21
updated: 2026-10-21
status: open
tags:
  - concurrency
  - crash
  - use-after-free
  - worker-pool
  - qg_keyboard
source:
  - FIX-UI-004 (incidental diagnosis)
  - src/workers/tasks.cpp
---

# Root Cause: Intermittent SEGFAULT in Worker-Pool Delivery (use-after-free on the receiver)

Related: [[Current-State]] · [[Known-Issues]] · [[Threading-Model]]

## Observed Failure

`ctest` reported `qg_keyboard` **SEGFAULT** after all 9 of its tests had passed
(the runner prints `Totals: 9 passed` first, then the crash, i.e. teardown).

Earlier project notes recorded this as "environmental, not code-related". That is
**wrong** — it reproduces in a plain sequential loop:

    for i in $(seq 1 25); do ./build/tests/qg_keyboard_test >/dev/null 2>&1 || fails=$((fails+1)); done
    → sequential non-zero exits: 1/25        (≈4 %)
    → under gdb: SIGSEGV on attempt 14 of 20 (≈5-7 %)

## Captured Frame

    Thread 11 "Thread (pooled)" received signal SIGSEGV, Segmentation fault.
    0x00007ffff23d95d8 in QObject::thread() const () from libQt6Core.so.6

The crash is on a **QThreadPool worker**, inside `QObject::thread()` — the first
call `QMetaObject::invokeMethod(contextObject, …)` makes on its context object.

## Mechanism

`src/workers/tasks.cpp` (`submitTask`) captures the receiver as a raw pointer and
invokes it from the pool thread:

    auto *runnable = new FunctionRunnable(
        [token, receiver, work, deliver]() mutable {
            ...
            QMetaObject::invokeMethod(receiver, [deliver]() { deliver(); },
                                      Qt::QueuedConnection);   // tasks.cpp:81
        });

The comment there claims the context object makes Qt drop the event if the
receiver is destroyed first. That is only true **if the receiver is still alive
when `invokeMethod` is called**. It does not protect the window between the task
being queued and the worker reaching that line: if the receiver (a controller /
service destroyed at shutdown or at test teardown) is freed in the meantime, the
worker dereferences freed memory to read its thread affinity → SEGFAULT.

`qg_keyboard_test` triggers it because it constructs keyboard/browser/playback
objects and destroys them at the end while pool tasks may still be in flight.

## Impact

Any receiver destroyed while a task is queued can crash the process: closing the
app, navigating away from a controller that was torn down, or the tail of any
test suite. It is intermittent (timing-dependent), which is why it presents as a
"flaky test" rather than an obvious bug.

## Proposed Fix (not applied — separate work item)

Make delivery ownership-safe rather than pointer-based, e.g.:

1. Deliver through a `QPointer<QObject>`/`QWeakPointer` captured by value and
   re-check on the GUI thread, or
2. `QMetaObject::invokeMethod(qApp, …)` (a process-lifetime context) and validate
   the receiver inside the delivered functor on the GUI thread, or
3. Give each receiver a `TaskToken`/generation that is cancelled in its
   destructor, and have workers check cancellation for *every* delivery.

Option 2/3 preserve the current queued-delivery contract while removing the
cross-thread dereference of a possibly-freed object. This needs its own tests
(destroy the receiver mid-task under ASan) and should not be bundled into a UI
geometry fix.

## Verification of the diagnosis

- `qg_keyboard_test` links `qg_playback`/`qg_core` only — **not** `qg_qml` — so
  QML changes (ContentView/Main) cannot influence it.
- Direct re-runs: 9/9 tests pass every time before the (sometimes) teardown crash.
- Crash frame is consistent: worker thread → `QObject::thread()`.
