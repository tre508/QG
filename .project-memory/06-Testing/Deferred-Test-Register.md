---
type: testing
project: QG
created: 2026-10-22
updated: 2026-10-22
status: active
aliases:
  - Deferred Tests
  - Deferred-Test-Register
tags:
  - testing
  - workflow
  - deferred
  - backlog
source:
  - /home/treshen/Projects/QG/DEFERRED_TESTS.md
  - Agent-Workflow.md
---

# Deferred Test Register

Vault mirror of the authoritative in-repo backlog:
**`/home/treshen/Projects/QG/DEFERRED_TESTS.md`**. That file wins if the two differ.
Policy: [[Agent-Workflow]] § Development Workflow Mode (2026-10-22).

## Why this note exists

Builds, CTest/unit/QML runs, launches, screenshots, viewport sweeps, benchmarks and
hardware validation are deferred to a testing phase the user authorizes. Instead of
running checks per fix, each fix contributes **one row** here; the batch phase
executes the grouped checklist once and upgrades items to `VERIFIED`.

## Register

| Fix ID | Changes Implemented | Static Review | Deferred Tests | Risk |
|---|---|---|---|---|
| FIX-VIS-013/014 | Path Bar density button derives icon + label from the active mode; mode-naming tooltip/a11y; strip options named with active chip and 24 px sizing | STATICALLY REVIEWED — `qmllint` no new errors; single shared state path unchanged | Cycle Gallery/Standard/Compact and capture each (Path Bar icon + strip chip agree); hover tooltips; `Ctrl+1/2/3`; folder-revisit persistence; `tst_pathbar`, `tst_contextstrip`, `tst_density`, `tst_main` | Low |
| FIX-VIS-011/012 | Card placeholder cues bound to `thumbnailState`; exclusive centre zone; amber failure scrim + compact Retry; fixed TL favourite/watched slots; 22 px TR hover chip; `Accessible.name` on hover actions + Retry; meta line anchored to the filename; `Theme.chipScrim` token | STATICALLY REVIEWED — `qmllint` clean apart from two stale-build `chipScrim` artifacts; zone geometry checked arithmetically at 168 px | Screenshot matrix (idle/hover/focus/selected/favourite/watched/partial/failed/denied) at 168/244/360 px card widths; `tst_videocard` re-run; full QML suite + CTest; confirm hover still does not select | Medium — layering/anchoring in a shared delegate |
| FIX-RUNTIME-002 | `PlaybackSession` Q_PROPERTY declarations (identity/path/duration/position/state/volume/muted/seekable/errorMessage) fix Stage.qml + Main.qml undefined bindings; shim keys changed from `type="i"` to `type="e"` enum with correct defaults; probe gets explicit bounded `probesize`/`analyzeduration`; CUDA/VDPAU documented as expected `auto-safe` discovery | STATICALLY REVIEWED — Q_PROPERTY READ/WRITE/NOTIFY match existing accessors/signals; FFmpeg dictionary consumed by `avformat_open_input` (no double-free); shim enum types match deprecated schema enum definitions | Startup no GSettings `g_settings_get_enum` CRITICALs; FFmpeg probe completes for MP4/MKV; Stage.qml/Main.qml no undefined-binding warnings; Stage open/close + playback state; software fallback when CUDA/VDPAU absent | Medium — QML type registration, schema enum resolution |
| **FIX-SRCH-01** (AUD-005, 2026-10-26) | `SearchController` publishes `active`/`summary`/`resultCount` as `Q_PROPERTY` (`activeChanged(bool)` kept, de-duplicated, member set before notify; `clear()` re-notifies `resultsChanged`; `setFolderName`/`setSortLabel` `Q_INVOKABLE`); the shell binds `manifestActive` to the property and reads the summary/applied query from it; §7.4 "Clear search" routed through the field (`PathBar.clearSearchQuery()` → `ContentView.searchClearRequested()`) | **STATICALLY REVIEWED** — `g++ -std=c++20 -fsyntax-only` pass with a red control (exit 1 on a deliberate rename); `qmllint 6.10.2` identical diagnostic sets vs the pre-fix worktree (128 PathBar / 74 ContentView / 139 Main); `tests/` untouched | Audit §Deferred verification verbatim (Ctrl+F → morph → Manifest list → `N results for …` summary → Esc restores scroll/selection → no-results state), plus focus ownership / second-Esc / §7.3 ↑↓ row focus, `qg_search_controller`, `tst_wiring` search cases, `tst_pathbar`, `tst_contentview`, `tst_overlays_dialogs`, full CTest + QML suite | **Medium** — the surface goes live for the first time; AUD-023 selection restore now user-visible |
| **FIX-PC-01** (AUD-007 half, 2026-10-26) | `FolderModel` publishes `folderPath`/`folderPathChanged` (from `FolderSnapshot::path`, inside the reset) and drops it in `clear()`; `PlaybackController::onModelStructureChanged()` compares the session's containing directory (`QFileInfo(session->path()).absolutePath()`) with the displayed folder and only stops when the session's folder is displayed and the identity is gone; a live session on another folder now leaves the context/Reel untouched (deviation from the audit patch — see the session note) | **STATICALLY REVIEWED** — `g++ -fsyntax-only` exit 0 on both TUs with red controls (renames → exit 1); `tests/` untouched | Start playback → navigate three folders away (playback + Stage must survive) → Reel must keep the playing folder's contents → return and confirm re-anchoring → delete the playing file while its folder is shown and confirm playback still stops → browse-back-after-removal nuance → `qg_reel_wiring`, `qg_playback_lifecycle`, full CTest + QML suite | **Medium** — changes the navigation/playback correlation; path equality assumes clean absolute paths; Reel header still shows the browsed folder name |
| **FIX-TH-01** (AUD-008, 2026-10-26) | `src/media/thumbnails.hpp` + `src/controllers/thumbnail_controller.{hpp,cpp}` — the persisted `thumbnails` row is now read before extracting: `consultQgCache()` (GUI thread) runs `ThumbnailRepository::lookup()` + `decideThumbnail()` and returns Serve/Terminal/Extract; the media pool reads the resolved PNG (`runExtraction`) instead of decoding; QG-cache hits are not re-encoded (`ThumbnailExtraction::CacheOrigin::QgCache`; a freedesktop hit is still written through); terminal failures are returned without a decode; `retry()` sets `manualRetry` so §18.5's escape hatch survives. **Deviations:** the audit's `failed.error = QStringLiteral(…)` does not compile (→ `Error::make`), `publishedFromCache(request)` is impossible (const ref → flag on the result), the audit's DB call inside `runExtraction()` would use the single thread-affine `Database` connection from the media pool (→ DB on the GUI thread, PNG read on the pool), and a single `fromCache` bool would have re-used a stale-but-same-key QG file for a freedesktop hit and broken `test_thumb_delivery`'s delivered-image assertion (→ origin enum, reuse keyed on `QgCache`) | **STATICALLY REVIEWED** — `g++ -std=c++20 -fsyntax-only` exit 0 on both TUs with five red controls (audit's `error` line, decision call, `manualRetry`, `cacheOrigin`, `cachedPngPath`); no DB access left in `runExtraction()`; `composition.cpp:103-106` confirms production wiring; no thumbnail test sets a database → their extraction counts are unchanged; `tests/` untouched | 200-video folder: generate → navigate away → return and confirm no FFmpeg open (extractor counter / `strace -f -e openat`) and a prompt second paint; corrupt file: bounded retries, terminal after the third failed decode, no further decode across navigations; source change clears the marker; Retry still re-extracts; `qg_thumb_wiring`/`qg_thumb_delivery`/`qg_thumb_failure`/`qg_thumb_scheduler`/`qg_card_actions` re-runs; full CTest + QML suite; GUI-thread responsiveness with one SELECT per requested row | **Medium** — the serve/extract decision moves, a DB read joins the GUI thread, terminal/retry semantics become observable |
| **FIX-AUD-006** (AUD-006, 2026-10-26) | `src/controllers/state_controller.{hpp,cpp}` — `StateController` subscribes to the canonical `FolderModel`'s `modelReset`/`rowsInserted` signals and applies persisted favorite/watched/progress to every row as it enters the model (initial load, navigation, refresh), exactly once per identity per generation; `scheduleLoad()` coalesces batched insertions into one zero-delay `QTimer::singleShot` pass; `m_applied` guards against re-decoration; `setModel()` disconnects the previous model, clears the guard, (re)schedules restoration; `loadIntoModel()` kept as the immediate entry point | **STATICALLY REVIEWED** — root cause confirmed at `src/core/composition.cpp:98`: one-shot `loadIntoModel()` runs before the first `navigate()` populates the model and never repeats; nothing hooks model population. `setModel()` body moved header→cpp with signal wiring; audit's `applyStateToRow` does not exist in this tree and was not added. `tests/` untouched | First folder shows favorites/watched/positions immediately; second decorated folder and refreshes restore them; toggle + restart preserves them (write path covered by `qg_card_actions`); first-paint comparison on a 5,000-entry folder before/after — O(n) `updateFavorite` scans remain until the identity index (AUD-024 in the audit; AUD-025 in this tree) lands | Low |
| **FIX-STAGE-01** (AUD-010 half, 2026-10-27) | `qml/QG/Main.qml` — `windowedVisibility` property + `onCinemaActiveChanged` now fullscreens the existing window on Cinema entry (remembering the prior state unless already fullscreen), focuses the Stage, wakes the transport overlays, and restores the remembered state on exit; `import QtQuick.Window` | STATICALLY REVIEWED — `qmllint` control pair (finished vs reverse-applied pre-fix file, siblings present): 141 vs 139, only 2 × `wakeControls not found on "Stage"` which the pre-fix file already emits twice (no `qmldir` → local types unresolved); no `Window`/`visibility` diagnostic; `git diff` = one file; `tests/` untouched | On X11: F11 removes decorations and fills the screen; Esc restores the previous geometry exactly; double-click toggles; playback does not restart or re-buffer; pointer + overlays hide after 2 s. Offscreen: assert the `window.visibility` **property** (fullscreen is a no-op there); maximized → Cinema → exit returns to maximized; Stage focus survives the state change; `tst_cinema` full re-run + full QML suite + CTest | **Medium** — first window-state manipulation; needs a real WM to prove |

## Executed before the policy (kept as regression re-runs)

| Fix ID | Result | Evidence |
|---|---|---|
| FIX-UI-003 | `qg_mounts` 14/14, `qg_places_model` 14/14, `tst_places_sidebar` 16/16, `tst_icon` pass | [[Current-State]] |
| FIX-UI-004 | `tst_grid_geometry` 13/13 across 7 widths | [[standard-grid-seven-columns]] |
| FIX-VIS-010 | `qg_thumb_delivery` 10/10 (real MP4+MKV), `tst_videocard` +3, full CTest 47/48 twice, Xvfb paint check 98.6 % | [[video-thumbnails-not-rendered]], [[Test-Results]] |
| FIX-WK-01 (AUD-002) | `qg_workers` 17/17; 200/200 plain + 125/125 ASan runs; pre-fix ASan control SEGV 3/3 | [[2026-10-25-fix-worker-delivery-uaf]] |
| FIX-PB-01 (AUD-001) | `qg_seek_policy` 15/15 + 25/25 repeats; `qg_transport_state` 10/10 on real media; ASan+UBSan clean | [[2026-10-25-fix-aud-001-seek-memory-safety]] |
| AUD-003 (file ops async half) | `qg_file_ops_async` 8/8 + 25/25 repeats + 20/20 ASan/UBSan; pre-fix control red 4/4 (818 ms block); full CTest 47/49 with only the two known host flakes | [[2026-10-25-fix-aud-003-file-ops-async]] |

## Known defects recorded, not scheduled

| Item | Where | Note |
|---|---|---|
| ~~Worker-pool delivery use-after-free~~ | `src/workers/tasks.cpp` | ✅ **Fixed + validated 2026-10-25** — hub + `QPointer` guard; pre-fix ASan control SEGV 3/3, post-fix 0 crashes in 200 plain + 125 ASan runs. See [[2026-10-25-fix-worker-delivery-uaf]] |
| ~~File operations on the GUI thread (AUD-003)~~ | `src/filesystem/operations.cpp` | ✅ **Fixed + validated 2026-10-25** (async dispatch) — `rename`/`move`/`copy`/`deletePermanently` now submit to the I/O pool; pre-fix control measured **818 ms** GUI-thread block on a 3 MB / 400-file copy, `qg_file_ops_async` 8/8 + 25/25 repeats + ASan/UBSan clean. See [[2026-10-25-fix-aud-003-file-ops-async]] |
| File-op progress, cancellation, §8.2 partial summary | `operations.cpp` | **Not started (FIX-FO-02):** needs a chunked copy with checkpoints; §8.1 stays PARTIAL. Cross-device `EXDEV` fallback (FIX-FO-01) is the same gap from the AUD-019 side |
| 4 GB copy / live-desktop drag responsiveness (V9) | `qg_file_ops_async` covers 3 MB; no GUI automation | Deferred: needs a desktop session ([[Session-Handoff]]) |
| Cross-thread delivery on mpv/render threads | `video_surface.cpp:99-101`, `mpv_backend.cpp:84`, `events.cpp:23` | Same class as the worker-delivery UAF, other threads/owners; **not** covered by that work item — each needs its own item + tests ([[worker-pool-receiver-use-after-free]]) |
| Warm start re-extracts thumbnails | `thumbnail_controller.cpp` | `decideThumbnail()` / `ThumbnailRepository::lookup()` have no callers, so the QG cache is write-only (DESIGN §18.2 "Serve") |
| Probe-cache invalidation on source change | probe chain | THUMB-007 half covered |
| `ContentView` undeclared `focusIndex` | `ContentView.qml` | ✅ **Fixed 2026-10-25** (FIX-SEL-01): declared as the legacy cursor; the live path reads the `selFocusIndex` adapter ([[2026-10-25-fix-aud-004-selection-focus]]) |
| Stage drawn over the grid (AUD-009) | `ContentView.qml` / `Main.qml` | ✅ **Fixed 2026-10-25** (FIX-STAGE-01, reflow half): `reservedTop` inset applied to grid/compact/Manifest, shell binds it to `stage.height + Theme.space3`; harness `aud009_reflow` 6/6 ([[2026-10-25-fix-aud-009-stage-reflow]]). **Re-verified 2026-10-26** by inspection against the FIX-STAGE-01 prompt: all four code items and the reporter fix are present, `Stage.qml` never reads `contentView`; no code change needed. **Deferred:** Stage positioned at the playing item; live-desktop play-through. **Gate gap:** the harness is outside the repo, so `ctest` does not cover the inset |
| `reportVisibleRange` double-subtracted the top margin | `ContentView.qml` | ✅ **Fixed 2026-10-25** (same change): with the ~400 px Stage inset it emitted no range at all for the first screen |
| Four `qg_qml` subtests | CardActions, ContentView, Keyboard, TransportBar | Pre-existing baseline failures |

## Batch phase checklist (when authorized)

1. Clean build: `cmake -S . -B build -G Ninja && cmake --build build`.
2. `TMPDIR=/tmp ctest --test-dir build --output-on-failure` → expect **49 tests**
   (was 48 before `qg_file_ops_async`; `qg_seek_policy` was added 2026-10-25 too),
   with only the known host flakes failing (`qg_watcher`,
   `qg_video_surface_offset`) and the four known `qg_qml` subtests; if
   `qg_keyboard` SEGFAULTs, re-run it directly (its teardown UAF was fixed
   2026-10-25, so any crash now is a **new** finding).
3. Full QML suite → expect 203 passed / 4 known failures.
4. Focused re-runs: `qg_thumb_delivery`, `qg_thumb_wiring`, `qg_thumb_scheduler`,
   `qg_thumb_failure`, `qg_mounts`, `qg_places_model`, `tst_grid_geometry`,
   `tst_places_sidebar`, `tst_videocard`.
5. Cold cache (delete cache dir) → PNGs appear, rows show imagery; warm relaunch →
   no re-extraction of unchanged sources.
6. Rapid navigation + scrolling → no stale thumbnails, GUI responsive.
7. Xvfb visual sweep at 1833 / 1500 / 1150 / 1100 / 900 / 820 / 700 px.
8. Hardware: real USB drive + disposable mount for eject (still unverified).

## Rules

- Never report a deferred check as passed.
- Never omit a deferred check from this register.
- Never delete existing tests to simplify an implementation.
- Group related checks; do not duplicate the same test across fixes.
