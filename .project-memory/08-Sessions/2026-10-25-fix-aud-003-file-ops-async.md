---
type: session
project: QG
created: 2026-10-25
updated: 2026-10-25
status: complete
aliases:
  - Fix-AUD-003-File-Ops-Async
tags:
  - session
  - performance
  - threading
  - file-operations
  - aud-003
source:
  - proj-audit.md §7 (AUD-003)
  - DESIGN.md §17.6, §15.3, §8.1, §22.1
  - src/filesystem/operations.{hpp,cpp}
  - src/controllers/actions_controller.cpp
  - tests/unit/test_file_ops_async.cpp
  - ctest + ASan runs (2026-10-25)
---

# 2026-10-25 — AUD-003: file operations on the I/O pool

## Objective

Resolve AUD-003 ("every file operation runs synchronously on the GUI thread") in
the checked-out tree, verifying the finding's claims rather than applying its
patch text verbatim, and validate the result with a pre-fix control run and
repeat/sanitizer runs.

## Verified root cause

**CONFIRMED, with the cost measured rather than assumed.** At HEAD,
`operations.cpp:13-27` was four one-liners of the shape

    void FileOperationsService::copy(...) { emitFinished("copy", copySync(paths, targetDir)); }

so the whole walk/copy/delete ran inside the `Q_INVOKABLE` call on the GUI
thread, and `emitFinished` re-entered QML before the call returned. The new
suite's watchdog case, run against the unmodified `operations.cpp`, measured the
block directly:

| Fixture | Pre-fix GUI-thread block | Post-fix |
|---|---|---|
| 24 MB / 3,000 files (triage run) | **6,027 ms** | call returns in < 1 ms, loop keeps turning |
| 3 MB / 400 files (final fixture) | **818 ms** | call returns in < 1 ms, watchdog ticks |
| 150 files / 600 KB (receiver-death case) | copied inline | copied on the pool, result dropped |

The audit's other claims checked out: `copyDirRecursive` is an unbounded
recursive walk, `reveal()`/`openWith()` block in `QProcess::execute`, and
`moveSync` still aborts the batch on the first failure (`break`, line 120) — that
last one belongs to AUD-019 and was deliberately left alone.

## Changes

| File | Change |
|------|--------|
| `src/filesystem/operations.cpp` | `rename`/`move`/`copy`/`deletePermanently` submit to `qg::WorkerPools::io()` at `qg::TaskPriority::Normal` via `qg::submitTask(pool, priority, nullptr, this, work, deliver)`; `work` runs the unchanged `*Sync` helper into a `std::make_shared<OperationResult>()`, `deliver` calls `emitFinished` on the GUI thread. `reveal`/`openWith`/`showProperties` intentionally unchanged (short-lived launchers, out of scope per the finding) |
| `src/filesystem/operations.hpp` | Comment corrected: the header claimed "move/copy chunked with cross-device fallback" and "cancellation checkpoints" — neither exists. It now documents the async facade, the `*Sync` helpers as its payload, that results arrive in *completion* order, and that FIX-FO-01/FIX-FO-02 are the follow-ups |
| `tests/unit/test_file_ops_async.cpp` (new) | 8 cases: delivery is not inline and happens exactly once; delivery lands on the GUI thread; the event loop keeps turning during a real 400-file copy (1 ms watchdog); all four entry points are routed; `*Sync` helpers unchanged (including the missing-path failure); a service destroyed with its work in flight is never delivered to |
| `tests/CMakeLists.txt` | `qg_file_ops_async` target (headless, `QT_QPA_PLATFORM=offscreen`) |
| `tests/unit/test_action_feedback.cpp` | 7 cases that asserted inline completion now wait for the reported outcome (`QTRY_COMPARE` on the spy) or the effect (`QTRY_VERIFY` on the filesystem); `clipboardCutPasteMovesFile` now asserts the `move` result it was already spying for |
| `tests/unit/test_card_actions.cpp` | `renameReachesTheFileSystem` waits for the rename effect instead of assuming it |

Public API unchanged: no new signal, no new parameter, no QML change. Because the
delivery hop is a queued invocation, a result can only be observed after the
event loop turns — `ActionsController` and `Main.qml` need no edit, and the QML
rename conflict path (`Main.qml:247-263`) already decides from
`targetPathForRename`/`pathExists` *before* dispatching.

## Verification (all executed)

Host: Qt runtime only, no dev packages, no sudo → user-local Qt prefix at
`~/.hermes/cache/scratch/qtdev/prefix/usr` (`CMAKE_PREFIX_PATH`) plus
`LD_LIBRARY_PATH`. Trees: `build-work` (plain + ASan+UBSan `build-work-asan`).

```bash
export CMAKE_PREFIX_PATH=/home/gayzer/.hermes/cache/scratch/qtdev/prefix/usr
export LD_LIBRARY_PATH=/home/gayzer/.hermes/cache/scratch/qtdev/prefix/usr/lib/x86_64-linux-gnu:/home/gayzer/.hermes/cache/scratch/qtdev/prefix/usr/lib/qt6/lib
export DISPLAY=:77   # an Xvfb from the same prefix is kept running on :77
```

`tests/run_gl_test.sh` **silently `exit 0`s** on this host (`Xvfb` is not on
`PATH`, so it takes the "SKIP: no DISPLAY and Xvfb not installed" branch) — ctest
then reports the GL suites as sub-second passes that never ran. Running the
binaries directly with `DISPLAY=:77 QT_QPA_PLATFORM=xcb` (or exporting `DISPLAY`
before ctest) is what makes them real; `qg_card_actions` takes ~0.1 s when it
runs and ~0.01 s when it is skipped.

| Check | Command | Result |
|---|---|---|
| Pre-fix control | final suite vs. `git show HEAD:src/filesystem/operations.cpp` | **4/4 discriminating cases red** — `delivered == 1` immediately (synchronous echo), 818 ms block on the 400-file copy; the 4 contract cases (delivery thread, `*Sync` helpers) pass on both trees, as intended |
| New suite | `qg_file_ops_async` | **8/8**, and **25/25** consecutive runs |
| ASan+UBSan | `build-work-asan` `qg_file_ops_async` | **20/20 clean**, 0 UAF, 0 leaks, 0 UBSan findings (includes 15 repeats of the receiver-death path) |
| ASan+UBSan (wiring) | `qg_action_feedback`, `qg_card_actions` | both clean |
| Full CTest | `DISPLAY=:77 ctest --test-dir build-work -E ^qg_qml$` | **47/49** — only the two pre-existing host-flaky suites (`qg_watcher`, `qg_video_surface_offset`); no new failure |
| QML suite | `QT_QPA_PLATFORM=offscreen ./build-work/tests/qg_qml_test -input tests/qml` | **204 passed / 4 failed**, the four documented baseline subtests (CardActions keyboard focus, ContentView focus/selection, Keyboard duplicate `Alt+Left`, TransportBar icon state) — no new failure (the change is C++-only and the QML suites drive mock controllers) |
| Build | `cmake --build build-work` | all targets; zero warnings from the touched files |

### Pre-existing failures (not caused by this work)

| Test | Control evidence |
|---|---|
| `qg_watcher` (`modelDiffPreservesUnaffectedRows`) | Re-run with the **pre-change binary in `build/`**: identical failure (`removed.count() == 0`, expected 1) — that path contains no file operation |
| `qg_video_surface_offset` (`decodedFramesRenderAtItemRect`) | Documented software-GL flake (0 vs 6592 bright pixels); the AUD-001 session recorded the same split on the pre-change binary |

## Residual risks / not done here

- **Ordering is completion order, not request order.** A caller that keeps
  per-request state can observe a later operation's result while an earlier one
  is still in flight. The one place with such state is `Main.qml`'s
  `pendingRename*` block: two renames of the *same* file issued within the pool
  latency window could have the first result apply the second's target
  (`renameSync` never overwrites, so the worst case is a spurious model mapping
  and a "Rename failed" toast, not data loss). Correlating the pending state with
  a request id is the clean fix and is **not** implemented; recorded here rather
  than suppressed.
- **No progress, no cancellation, no partial-failure summary** — unchanged by
  design: chunked copy with checkpoints + status-strip progress (§8.1, §15.4,
  §8.2 "Moved 9 of 10 …") is FIX-FO-02, and the cross-device (`EXDEV`) fallback
  that makes the partial summary reachable is FIX-FO-01. Both remain open.
- **Still synchronous on the GUI thread:** `reveal`/`openWith` (`QProcess`
  blocking launch) — deliberately out of scope per the finding; `TrashService`
  is called inline by `ActionsController::trash` and was not touched.
- **A 4 GB copy and drag-the-window interactivity on a live desktop session were
  not executed** (no GUI automation on this host); the responsiveness contract is
  covered at the 3 MB / 400-file scale instead.
- **TSan not re-attempted:** the previous session established that reports land
  inside uninstrumented `libQt6Core` and that the TSan runtime aborts on its own
  CHECK on this host, so it yields no verdict. ASan+UBSan is the usable
  instrument here.
