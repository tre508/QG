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

## Executed before the policy (kept as regression re-runs)

| Fix ID | Result | Evidence |
|---|---|---|
| FIX-UI-003 | `qg_mounts` 14/14, `qg_places_model` 14/14, `tst_places_sidebar` 16/16, `tst_icon` pass | [[Current-State]] |
| FIX-UI-004 | `tst_grid_geometry` 13/13 across 7 widths | [[standard-grid-seven-columns]] |
| FIX-VIS-010 | `qg_thumb_delivery` 10/10 (real MP4+MKV), `tst_videocard` +3, full CTest 47/48 twice, Xvfb paint check 98.6 % | [[video-thumbnails-not-rendered]], [[Test-Results]] |

## Known defects recorded, not scheduled

| Item | Where | Note |
|---|---|---|
| Worker-pool delivery use-after-free | `src/workers/tasks.cpp:81` | ~4 % teardown SEGFAULT; needs its own ASan-backed fix — [[worker-pool-receiver-use-after-free]] |
| Warm start re-extracts thumbnails | `thumbnail_controller.cpp` | `decideThumbnail()` / `ThumbnailRepository::lookup()` have no callers, so the QG cache is write-only (DESIGN §18.2 "Serve") |
| Probe-cache invalidation on source change | probe chain | THUMB-007 half covered |
| `ContentView` undeclared `focusIndex` | `ContentView.qml` | Legacy no-`selectionState` branches; dead in production |
| Four `qg_qml` subtests | CardActions, ContentView, Keyboard, TransportBar | Pre-existing baseline failures |

## Batch phase checklist (when authorized)

1. Clean build: `cmake -S . -B build -G Ninja && cmake --build build`.
2. `TMPDIR=/tmp ctest --test-dir build --output-on-failure` → expect 48 tests,
   only the four known `qg_qml` subtests failing; if `qg_keyboard` SEGFAULTs,
   re-run it directly to confirm the known flake.
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
