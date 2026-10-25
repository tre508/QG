---
type: testing
project: QG
created: 2026-10-20
updated: 2026-10-22
status: frozen
tags:
  - testing
  - results
  - evidence
source:
  - GUIDE_STATE.md
  - ctest output (2026-10-22)
  - DEFERRED_TESTS.md
---

# Test Results Evidence

> **Frozen 2026-10-22 by the deferred-testing policy.** No new runs are recorded
> here until the user authorizes a batch testing phase. Required future checks live
> in `DEFERRED_TESTS.md` (repo) and [[Deferred-Test-Register]] (vault). The numbers
> below are the last **actually executed** results — do not extrapolate from them.

## Last Executed Full Run (2026-10-22, pre-policy)

**Command**: `TMPDIR=/tmp ctest --test-dir build --output-on-failure`
**Result**: **47/48 PASS (98 %)**, confirmed by two independent runs
**Durations**: 172.3 s and 181.5 s

| Failure | Type | Detail |
|---------|------|--------|
| `qg_qml` | 4 pre-existing subtests | `CardActions::test_keyboardDispatchesFocusedCardActions`, `ContentView::test_focusAndSelectionAreDistinct`, `Keyboard::test_noDuplicateEnabledSequences` (`Alt+Left`), `TransportBar::test_reflectsPlaybackState` — baseline failures, unrelated to recent work |

`qg_keyboard` passed in both runs. It remains **intermittently** crash-prone: a
use-after-free in worker-pool delivery (`src/workers/tasks.cpp:81`) makes teardown
SEGFAULT in roughly 4 % of runs (1 of 25 sequential, 1 of 14 under gdb). See
[[worker-pool-receiver-use-after-free]].

### QML suite (same day)

**Command**: `QT_QPA_PLATFORM=offscreen ./build/tests/qg_qml_test -input tests/qml`
**Result**: **203 passed / 4 failed** (the four subtests above). Up from 187 before
FIX-UI-004 and FIX-VIS-010 added their suites.

### Targeted evidence produced by recent fixes

| Suite | Result | What it proves |
|-------|--------|----------------|
| `tst_grid_geometry` | 13/13 | Columns == laid-out columns at 7 widths; gutters equal; 16:9 and metadata band intact ([[standard-grid-seven-columns]]) |
| `tst_places_sidebar` | 16/16 | Sidebar rows reachable through the scroll model; headers as rows exact |
| `qg_mounts` / `qg_places_model` | 14/14 / 14/14 | Mount classification by identity; sidebar roles |
| `qg_thumb_delivery` | 10/10 | Real MP4+MKV: role is an `Image.source` URL, freedesktop cache hit skips decode, failures explicit, folder rows never extracted ([[video-thumbnails-not-rendered]]) |
| `tst_videocard` | 13/13 | Delegate paints a URL thumbnail (`status === Ready`, `paintedWidth > 0`) |

### Real-application evidence (Xvfb, 2026-10-22)

`Xvfb :98 / :99` at 1833x1080 with a real `qg` binary, temp HOME/XDG roots and
FFmpeg-generated media:

- FIX-VIS-010 colour-coded clip (1 s per colour): cached frame 320x180 **100 % red**;
  screenshot painted block **214x125 px at 98.6 % red** — the on-screen pixels are
  the extracted source frame. DB row `state=ok`, `source_size` correct.
- Cold and warm cache runs produced identical thumbnails (no stale imagery).

This is the only visual evidence captured so far. Wider screenshot comparison
(seven viewport widths) has **not** been performed.

## Historical Results

| Date | Result | Notes |
|------|--------|-------|
| 2026-10-22 | 47/48 (98 %) | Only `qg_qml` (4 subtests) |
| 2026-10-20 | 44/46 (96 %) | `qg_qml` + `dragdrop` — both then failed to *compile* (`PlacesSidebar.qml` `horizontalAlignment` on `Icon`); that compile break is fixed and `qg_qml` now runs 203 tests |
| 2026-10-19 | 43/45 (96 %) | Same 2 infra failures |
| 2026-10-18 | 43/45 (96 %) | Same 2 infra failures |
| 2026-10-14 | 43/45 (96 %) | Same 2 infra failures |
| 2026-10-12 | Baseline | Initial architecture audit |

## Reproduce (only in an authorized testing phase)

```bash
cd /home/treshen/Projects/QG
cmake --build build
TMPDIR=/tmp ctest --test-dir build --output-on-failure
QT_QPA_PLATFORM=offscreen ./build/tests/qg_qml_test -input tests/qml
```

## Evidence Files

- `build/Testing/Temporary/LastTest.log`
- `build/Testing/<date>/<test>.log`
- `tests/run_gl_test.sh` output
- Xvfb captures: `/tmp/qg_vis010_colour.png`, `/tmp/qg_vis010_cold.png`,
  `/tmp/qg_vis010_warm.png` (temporary paths — re-run to regenerate)
