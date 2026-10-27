---
type: session
project: QG
created: 2026-10-25
updated: 2026-10-25
status: complete
aliases:
  - Fix-AUD-001-Seek-Memory-Safety
tags:
  - session
  - playback
  - memory-safety
  - aud-001
source:
  - proj-audit.md §6 (AUD-001)
  - DESIGN.md playback §3.1
  - src/playback/playback_manager.cpp
  - qml/QG/playback/{Stage,TransportBar}.qml
  - tests/unit/test_seek_policy.cpp
---

# 2026-10-25 — AUD-001: seek memory safety, async dispatch and coalescing

## Objective

Resolve AUD-001 (dangling seek arguments, synchronous seek dispatch, inconsistent
non-seekable handling) in the checked-out tree, verifying the audit's claims rather
than applying its (corrupted) patch.

## State inherited from the tree

The committed `src/playback/playback_manager.cpp` does **not compile**: the
`seekAbsolute()` region contains an interrupted comment and an entire
`actualPosition()` definition spliced into its middle (`return sing the call, …`).
An uncommitted working-tree repair already existed when this work item started
(it de-indented that region, moved `actualPosition()` out, and gave the seek
argument a named `QByteArray`). That repair was preserved and finished here; the
audit's "corrupted replacement" is this committed corruption.

## Verified root cause

**Defect A — dangling argument pointer: CONFIRMED.** The two-statement form

    const char *args[] = {"seek", QByteArray::number(target).constData(), …};
    mpv_command(handle, args);              // args[1] is freed by now

destroys the temporary at the end of the *initialisation* statement, so the
pointer stored in `args[1]` is dangling before the call on the next line. Verified
independently with a standalone ASan probe (no libmpv needed):

    ==91307==ERROR: AddressSanitizer: heap-use-after-free … READ of size 5
    #0 strlen … #1 consume(dangling_probe.cpp:6) #2 main
    freed by … QTypedArrayData<char>::deallocate (qarraydata.h:162)

**Defect B — synchronous dispatch on the GUI thread: CONFIRMED as documented.**
libmpv's own header: *"Synchronous calls have to wait until the playback core is
ready, which currently can take an unbounded time (e.g. if network is slow or
unresponsive)"* — and `seek()`/`seekAbsolute()` are `Q_INVOKABLE`, i.e. called from
QML/keyboard on the GUI thread. The working tree already switched to
`mpv_command_async`; that is kept.

**Defect C — seekability policy: PARTIALLY CONFIRMED.** Both C++ entry points
already consulted `PlaybackSession::seekable()`, but the QML half was missing:
`PlaybackManager::seekRejected()` was never connected to
`TransportBar::flashSeekRejected()`, so a refusal from the manager (keyboard ±10s,
0-9 jumps, resume) produced no visible feedback, while a scrub on a non-seekable
stream flashed amber — two different behaviours for one rule.

**Defect D — consolidation: CONFIRMED missing.** DESIGN.md playback §3.1.4 requires
"Rapid successive seek requests are consolidated, prioritizing the latest requested
position". The tree relied on `mpv_command_async` alone; asynchronous submission
queues *every* request, so holding an arrow key (or scrubbing) issued a stream of
seek commands, and there was no supersession, no NaN/inf/duration normalisation,
and no reconciliation between the optimistic indicator and observed positions.

## Changes

| File | Change |
|------|--------|
| `src/playback/playback_manager.cpp` | `normalizeSeekTarget()` (NaN→0, ±inf→0/duration, clamp to known duration); `requestSeek`/`flushSeek`/`onSeekWindowClosed` consolidation; `dispatchSeek()` (injected dispatcher or `mpv_command_async` with a named argument buffer); `rejectSeek()` (restores `actualPosition()`, drops the pending burst, emits `seekRejected()`); `intendedPosition()` for relative accumulation; `time-pos` reconciliation (optimistic value survives stale observations, reality wins on landing or after `kSeekSettleMs`); `writePosition()` no longer clobbers an in-flight target; intent cleared on open/stop/backend change/file load; `actualPosition()` guards non-finite/unavailable values; `applyResume()` no longer publishes a position a refused seek never reached |
| `src/playback/playback_manager.hpp` | `SeekDispatcher` + `setSeekDispatcher()` (injected seam, same style as `setEnricher`/`setEjector`/`setPositionLoader`), `normalizeSeekTarget()`, the three seek constants, and the seek-state members |
| `qml/QG/playback/Stage.qml` | `Connections { target: stage.playbackManager; function onSeekRejected() { transport.flashSeekRejected() } }` — the manager keeps the policy, the bar only shows the affordance; a null target stays a no-op |
| `tests/unit/test_seek_policy.cpp` (new) | 15 tests: normalisation, duration clamping, `+inf` refusal without a duration, non-finite offsets, silent no-op with no backend, rejection without touching the backend, consolidation to the latest target, relative accumulation, clamp at zero, pending-burst drop on rejection, stale-observation immunity, landing, settle deadline |
| `tests/CMakeLists.txt` | `qg_seek_policy` target (headless; injected dispatch, no render context or media) |
| `tests/qml/tst_stage.qml` | +1 test: a refused seek reaches the transport bar's amber affordance; detaching the manager stays safe |

## Design notes

* **Consolidation shape**: leading edge + trailing flush. The first request of a
  burst is issued immediately (no added latency for a single ±10s press); further
  requests inside `kSeekCoalesceMs` (120 ms) collapse into one pending slot and
  the *newest* target is flushed when the window closes. Requests cannot
  accumulate, and one burst cannot flood mpv.
* **Relative seeks accumulate on the intended position**, not on the last observed
  one, so three rapid `+10s` presses yield +30s even while mpv still reports the
  pre-seek position.
* **Optimistic indicator is bounded**: the intended position owns the display only
  until an observation lands within `kSeekSettleToleranceSeconds` (0.35 s) or
  `kSeekSettleMs` (2 s) elapses, so a seek that cannot be reached cannot pin the UI
  (§3.1.2 vs §3.1.5).
* **`absolute+exact`** is kept: `exact` is what makes the final position match the
  request (§3.1.5) instead of snapping to a keyframe; the visible cost is decoding
  from the previous keyframe, which is the behaviour this project already had.
* **mpv ordering caveat**: libmpv documents that *"asynchronous calls may be
  reordered in arbitrarily with other synchronous and asynchronous calls"*. The
  window guarantees at most one seek command per window and that the newest target
  is dispatched last, but strict ordering would require serialising on
  `MPV_EVENT_COMMAND_REPLY`. Recorded as a residual risk, not suppressed.

## Verification (all executed)

The host has Qt runtime packages but no dev packages and no sudo, so a user-local
Qt dev prefix was extracted with `apt-get download` + `dpkg-deb -x` and pointed at
with `CMAKE_PREFIX_PATH`; `Xvfb` (+ `xserver-common`, `xkb-data`) was extracted the
same way, which is what lets the GL/playback tests run for real instead of
QSKIPping. Trees: `build-work`, `build-work-asan`, `build-work-tsan`.

| Check | Command | Result |
|---|---|---|
| Build | `cmake --build build-work` | all targets, no warnings from the touched files |
| New policy suite | `qg_seek_policy` | **15/15**, and **25/25** consecutive runs |
| Real-media seek (GL, Xvfb) | `qg_transport_state` | **10/10** runs (open → play → `seekAbsolute` on a real ffmpeg-generated stream) |
| Full suite | `ctest --test-dir build-work -j 4 -E ^qg_qml$` | **46/48** — see "pre-existing failures" below |
| QML suite | `ctest -R ^qg_qml$` | **204 passed / 4 failed** (the 4 documented baseline failures; the count rose by 1 because of the new `tst_stage` test) |
| ASan+UBSan | `qg_seek_policy` 15/15 clean; `qg_transport_state` 3/3 clean **with real mpv seeking** | no SEGV, no use-after-free, no leak, no UB — this also empirically confirms the async argument buffer outlives mpv's later use of it |
| Defect A mechanism | standalone ASan probe of the original two-statement form | heap-use-after-free reproduced |

### Pre-existing failures (not caused by this work)

| Test | Evidence |
|---|---|
| `qg_watcher` (`modelDiffPreservesUnaffectedRows`) | flaky on this host; fails 25/25 standalone **with the pre-change binary** in `build/` too; no seek/submitTask involvement |
| `qg_video_surface_offset` (`decodedFramesRenderAtItemRect`) | flaky under software GL: 0 vs 6592 bright pixels; the **pre-change binary** shows the identical split (pass/fail/pass with the same 12.5 s failure path) |

Both are timing-sensitive tests that were previously invisible because the GL
suites QSKIPped without Xvfb.

## Follow-ups

- `qg_video_surface_offset` and `qg_watcher` are genuinely flaky on this host and
  deserve their own fix (frame-wait budget / mtime-resolution assumptions).
- Strict async ordering (reply-serialised seeks) is documented above as the
  alternative if a real reordering problem is ever observed.
- `applyResume()` now publishes only through `seekAbsolute()`: a resume on a
  non-seekable stream is refused **and** reported, instead of leaving the session
  claiming the stored position.