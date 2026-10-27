---
type: session
project: QG
created: 2026-10-25
updated: 2026-10-25
status: complete
aliases:
  - Fix-Worker-Delivery-UAF
tags:
  - session
  - concurrency
  - crash
  - worker-pool
  - lifetime
source:
  - src/workers/tasks.cpp
  - src/workers/tasks.hpp
  - src/search/recursive_search_service.cpp
  - tests/unit/test_workers.cpp
  - ctest + ASan runs (2026-10-25)
---

# 2026-10-25 — Worker-delivery use-after-free: fix + validation

## Objective

Verify and permanently resolve the reported use-after-free in the QG worker task
delivery mechanism (`QMetaObject::invokeMethod()` dereferencing a possibly
destroyed `QObject` from a pool thread), then validate it with repeat runs and
sanitizers.

## What was wrong

`submitTask()` queued the result **on the receiver itself** from the pool thread,
so the worker read the receiver's thread affinity — i.e. dereferenced the
receiver. Qt's "queued events are dropped when the receiver is destroyed"
guarantee only covers a receiver that is still alive *when the post happens*; it
does not cover the submission→delivery window, and it never covers the
dereference the post itself makes. Full analysis:
[[worker-pool-receiver-use-after-free]] (now RESOLVED).

Two failure modes, same defect: SEGV in `QObject::thread()` on the worker (the
2026-10-21 frame at ~4 % of `qg_keyboard` teardown runs) and SEGV in
`QCoreApplicationPrivate::notify_helper()` on the GUI thread when the queued
`QMetaCallEvent` addressed to freed memory is dispatched.

## Changes

| File | Change |
|------|--------|
| `src/workers/tasks.cpp` | `deliveryHub()` (one process-lifetime `QObject` on the GUI thread), `postToGui()` (guarded GUI-thread hop), `submitTask()` builds a `QPointer<QObject>` guard on the submitting thread, records `hasReceiver` separately, and never touches `receiver` off that thread again |
| `src/workers/tasks.hpp` | complete thread contract (calling thread, `work` thread, `deliver` thread, null-receiver behaviour, receiver-death behaviour, cancellation, affinity) + `postToGui()` |
| `src/search/recursive_search_service.{hpp,cpp}` | same-class call-site fix: the I/O-pool walk used `invokeMethod(this, …)` from the worker; now `postToGui()` + a by-value token copy instead of the `m_token` member |
| `tests/unit/test_workers.cpp` | +6 tests (17 total) — post-dispatch receiver death, several receivers destroyed with tasks in flight, null receiver **and** null token, `postToGui` GUI-thread/dropped/empty-guard |

Public API unchanged: `submitTask()` keeps its signature and both the null-receiver
and cancellation contracts; `postToGui()` is an addition.

## Verification (all executed, minimal build)

| Check | Result |
|---|---|
| Pre-fix control under ASan | SEGV **3/3** — the deterministic repro (`deliveryDroppedWhenReceiverDestroyed`) does catch the original defect |
| Full CTest | **47/48** on the final tree (46/48 on the pre-final-comment tree, same code); only pre-existing `qg_qml` (4 documented subtests). The `qg_watcher` `modelDiffPreservesUnaffectedRows` failure is flaky on this host and **fails 25/25 standalone with the pre-change binary in `build/` too** → not attributable to this change (that path contains no `submitTask` call) |
| 25× repeats, 8 suites | **200/200 exit 0, 0 signal deaths** (historical `qg_keyboard` teardown rate ~1/25) |
| ASan+UBSan, 25× (workers/keyboard/thumb_delivery + browser_controller/thumb_wiring) | **125/125 runs clean**: 0 SEGV, 0 UAF, 0 leaks, 0 UBSan findings; plus single clean ASan runs of `qg_probe_wiring` / `qg_mounts` / `qg_composition` |
| TSan (separate `-DQG_SANITIZE=thread` build) | Delivery tests pass; all reports are inside uninstrumented `libQt6Core` and **reproduce in a 15-line Qt-only program**, then TSan aborts on its own runtime CHECK → no usable verdict on this host |

Evidence logs: `$HOME/.hermes/cache/scratch/{repeat-*,asan-*}.log` (scratch is pruned
when idle; the tables above are the durable record).

## Building on this host (important for the next session)

The machine has Qt **runtime** packages but not the dev packages
(`qt6-base-dev`, `qt6-declarative-dev`, `qt6-svg-dev`, `libsqlite3-dev`), and no
sudo, so `cmake` cannot find Qt6 and `.venv`-independent builds fail. A user-local
prefix was extracted without root (`apt-get download` + `dpkg-deb -x` + symlinks
for the missing `syncqt`/`.so` entries) and pointed at with
`-DCMAKE_PREFIX_PATH=<prefix>/usr`, plus
`LD_LIBRARY_PATH=<prefix>/usr/lib/x86_64-linux-gnu:<prefix>/usr/lib/qt6/lib`
(needed at build time by `qmlimportscanner`/`qmlcachegen` and at run time by
`libQt6Test`).

The stale `build/` and `build-asan/` trees were produced on a different machine
(`CMAKE_HOME_DIRECTORY=/home/testuser/Apps/QG`) and CMake refuses to reuse them;
their binaries still run and are what the pre-change control runs used. New trees
were configured as `build-work`, `build-work-asan` (`address,undefined`) and
`build-work-tsan` (`thread`).

Proper fix for the host, if the missing packages are installed once:

    sudo apt install qt6-base-dev qt6-declarative-dev qt6-svg-dev libsqlite3-dev

## Not fixed here (recorded in the RCA)

`video_surface.cpp:99-101` (`onMpvUpdate` builds a `QPointer` from raw `userData`
on mpv's thread, plus a TOCTOU before `invokeMethod`), `mpv_backend.cpp:84`
(wakeup post from mpv's thread), `events.cpp:23` (`EventBus::post`, currently
unreachable) and `priority_pool.cpp:143` (`schedulePump`). Same class, other
threads/owners — each needs its own work item and tests.

## Follow-ups

- The four `qg_qml` baseline subtests and the `qg_watcher`
  `modelDiffPreservesUnaffectedRows` failure on this host are untouched by this
  work and still open.
- TSan is unusable as a gate here until a TSan-instrumented Qt exists; ASan+UBSan
  is the reliable instrument for this class of defect.
- Deferred-test policy: this work item was explicitly authorised to run its own
  suite; [[Test-Results]] stays frozen and was not updated.
