---
type: session
project: QG
created: 2026-10-25
updated: 2026-10-25
status: complete
aliases:
  - Fix-AUD-009-Stage-Reflow
tags:
  - session
  - stage
  - layout
  - qml
  - aud-009
source:
  - proj-audit.md §AUD-009 (FIX-STAGE-01)
  - DESIGN.md §1.2, §3.4; playback §2.1 (item 7), §2.2
  - qml/QG/ContentView.qml
  - qml/QG/Main.qml
  - qml/QG/playback/Stage.qml (read only)
  - ctest + qg_qml runs, reflow harness (2026-10-25)
---

# 2026-10-25 — AUD-009: the Stage occupies layout space, the grid reflows

## Objective

Resolve the AUD-009 half of FIX-STAGE-01 — "the Stage is an overlay; the grid never
reflows beneath it" — by giving `ContentView` a `reservedTop` inset that the shell
binds to the Stage's height, and prove the reflow, the reachability and the
restore-on-close behaviour with a harness that composes the real Stage.

## Verified root cause

**CONFIRMED, as the finding states.** `Main.qml` placed the Stage as a *sibling* of
`ContentView`, anchored top/left, so it painted over the grid: `ContentView` had no
notion of the Stage at all (`grep -n "stage\|reserved" ContentView.qml` matched
nothing), every presentation started at `anchors.topMargin: contentView.pad`, and
the grid's viewport therefore extended *behind* the player. Cards were covered, not
pushed, and the keyboard cursor could land on invisible ones. The stale
"Stage overlay:" comment at the Stage block confirmed the original design intent.

Dependency check (the finding's only precondition): `Stage.height` is bound to
`parent.height`/`parent.width` only (`Stage.qml:60-63`) and the file contains **no**
reference to `contentView`/`reservedTop`/`topInset` — so the inset cannot feed back
into the Stage's height and no binding loop is possible. The harness run produced no
`binding loop` diagnostics.

## Changes

| File | Change |
|------|--------|
| `qml/QG/ContentView.qml` | `property real reservedTop: 0` plus a derived `readonly property real topInset: pad + reservedTop`; applied as `anchors.topMargin` to **all three** presentations (grid, compact list, and the Manifest container that carries the sortable header + results, so the header shifts with the list); `onReservedTopChanged: reportVisibleRange()`; the stale `pad`-based arithmetic in the visible-range reporter replaced (see below) |
| `qml/QG/Main.qml` | `reservedTop: (window.sessionActive && !window.cinema && stage.visible) ? stage.height + Theme.space3 : 0` on the `ContentView` instantiation; the "Stage overlay" comment replaced with the inline-Stage wording and an explicit note that the Stage sizes itself from the region, never from the inset |

**Defect found by the harness and fixed here.** The grid's `reportVisibleRange()`
computed `firstRow = floor((contentY − margin)/rowH)` — subtracting the top margin
even though `contentY` is already measured from the content origin. With the old
24 px pad that was a harmless one-row slack; with a ~400 px inset it produced
`last < first` for a first screen shorter than a row, so the view emitted **no**
`visibleRangeChanged` at all — i.e. no thumbnail or probe requests for the first
screen while the Stage was open. The reporter now uses the visible content window
`[contentY, contentY + grid.height)` and reports rows 0–7 with the Stage open vs
0–15 with it released.

No change to the Stage's sizing math, its anchor, or its parenting (it stays a
sibling that simply reserves space through the inset), as the finding requires.

## Verification (all executed)

Host: Qt runtime only, no dev packages → headers from the user-local Qt prefix
(`~/.hermes/cache/scratch/qtdev/prefix/usr`), system Qt runtime libs, Xvfb `:77`.
Build tree `build-work`; env must be inline (a background call does not inherit the
session's `LD_LIBRARY_PATH`, and `qmlcachegen` needs it).

| Check | Command | Result |
|---|---|---|
| Reflow harness (real Stage + real ContentView, composed as Main composes them) | `qg_qml_test -input ~/.hermes/cache/scratch/aud009/aud009_reflow.qml` | **6/6** |
| ├ reservation | 1200×620 region, 40 entries | Stage keeps `min(0.6·h, 9/16·w + space4)` = **372 px**; `reservedTop` = **384** = `stage.height + Theme.space3`; `grid.y == topInset == pad + reservedTop == 408 ≥ 372` |
| ├ row 0 below the Stage | first card `mapToItem(view)` | top = **exactly `reservedTop + pad`** (408) → fully below the Stage; no card overlaps the band |
| ├ release | `stageShown = false` | `reservedTop == 0`, `grid.y == pad`, first card back at the top |
| ├ reachability | `positionViewAtEnd()` | last card instantiated, **no part of it under the Stage's band**, fully inside the viewport |
| ├ range reporting | `reportVisibleRange()` | with the Stage: rows 0–**7**; released: 0–**15** (more visible rows) |
| ├ restore | open → scroll 1400 → close → reopen | `contentY` and the selection (`id17`) survive both transitions |
| └ Cinema | `region.cinema = true` | `reservedTop == 0` while the Stage fills the region (700 px) |
| QML suites (geometry-sensitive first) | `-input tests/qml/tst_{grid_geometry,contentview,stage,virtualization}.qml` | 13/13 · 5/6 (pre-existing `ContentView::test_focusAndSelectionAreDistinct`) · 6/6 · 4/4 — as the finding predicted, `reservedTop == 0` keeps them unaffected |
| Full QML suite | `DISPLAY=:77 ./build-work/tests/qg_qml_test -input tests/qml` | **200 passed / 8 failed** — the identical failure set to the pre-change baseline (`aud004-prefix-qml.log`) |
| Full CTest | `ctest --test-dir build-work -E ^qg_qml$` | **48/49** — only the known host-flaky `qg_watcher::modelDiffPreservesUnaffectedRows` |
| Build | `cmake --build build-work` | clean; `ContentView_qml.cpp`/`Main_qml.cpp` regenerated |

## Residual risks / not done here

- **The Stage is still top-anchored**, not inserted at the *playing item's* grid
  position with the earlier rows above it (§3.4's stronger reading). The finding's
  own resolution targets the inset, which is what landed; "bloom in place at the
  item's position" needs a flow layout (a GridView delegate or a spacer) and is a
  separate change.
- **No live-desktop or rendered-pixel evidence**: the harness asserts exact
  geometry in-process, but no video was played in a real window (no GUI automation
  on this host) and no screenshot of a reflowed grid was captured. The Stage's own
  entry animation/opacity is untouched.
- **Cinema fullscreen (AUD-010), Esc semantics (AUD-011), the entry fade (AUD-030)**
  and the token/enum cleanup remain open under FIX-STAGE-01; only the AUD-009 half
  was in scope.
- **`Theme.space3` is baked into the shell's expression**, so the visible gap under
  the Stage is a theme constant; if the Stage ever grows a shadow or an inline
  header the constant must be revisited.

## Re-verification 2026-10-26 (FIX-STAGE-01 prompt, no code change)

The roadmap's FIX-STAGE-01 prompt was re-issued for the AUD-009 reflow. Checked against
the tree before changing anything — **every required change is already present**, so no
edit was made (only records were touched):

| Prompt requirement | Where it lives |
|---|---|
| `property real reservedTop: 0` in `ContentView` | `ContentView.qml:124`, with `readonly property real topInset: pad + reservedTop` at `:128` so the sum exists in exactly one place |
| Inset applied to all three anchor blocks | `ContentView.qml:581` (grid), `:683` (compact list), `:727` (the whole Manifest container — header, list, no-results overlay and summary bar); these are the only `topMargin` sites in the file |
| Shell binds `reservedTop` to the Stage | `Main.qml:622-623` — `(window.sessionActive && !window.cinema && stage.visible) ? stage.height + Theme.space3 : 0` |
| Stage comment documents the flow | `Main.qml:777-783` ("a real element of the content flow … never from the inset, so no binding loop exists") |
| Visible-range reporter uses content coordinates | `ContentView.qml:312-326` — `[grid.contentY, grid.contentY + grid.height)`, with the double-subtraction history in the comment; `onReservedTopChanged: reportVisibleRange()` (`:344`) re-reports when the inset changes |
| `Stage.height` must not read `contentView` | `playback/Stage.qml` contains **no** reference to `contentView`/`reservedTop`/`topInset`; its size is `cinema ? parent.height : Math.min(parent.height * 0.6, parent.width * 9/16 + Theme.space4)` (`:55-62`), i.e. parent-relative only |
| Cinema reserves nothing | the binding's `!window.cinema` term, plus `cinema ? parent.height` in the Stage |

`Theme.space3` exists (`Theme.qml:70` = 12). No other QML file top-margins against the
Stage.

**Gap found by this pass:** the acceptance harness `aud009_reflow` (6 assertions: reservation
equals `stage.height + space3`, `grid.y == topInset`, first row top == `reservedTop + pad`,
no session → 0, scroll reaches the last card, close restores scroll+selection, Cinema
reserves 0) exists **only** outside the repository at
`~/.hermes/cache/scratch/aud009/aud009_reflow.qml`. `grep -rln "reservedTop\|topInset\|aud009" tests/`
returns nothing, so the inset has **no in-tree gate** — a reflow regression would not be
caught by `ctest`. Promoting the harness into `tests/qml/` (or adding its geometry
assertions to `tst_contentview.qml`) is the missing permanent gate; recorded in
`DEFERRED_TESTS.md`. Note `tests/qml/tst_stage.qml`'s working-tree change is the AUD-001
refused-seek test, not an AUD-009 gate.

**Run authorization:** the prompt's items 4–5 (QML tests + the ~40-entry harness) were
offered to the user on 2026-10-26 and **declined** — the standing logic-first policy stays
in force, so every check above is inspection-only and the gates stay in
`DEFERRED_TESTS.md`. Nothing was built, run or launched for this re-verification.

## Process note (unrelated to the fix, but recorded because it cost a build)

At **15:19:57** the whole `tests/` tree (72 tracked files plus the untracked
`test_file_ops_async.cpp`/`test_seek_policy.cpp`) was moved to the freedesktop
Trash (`/home/gayzer/.local/share/Trash/files/tests`, info file
`tests.trashinfo`) by something outside this session. Consequence: a later
`cmake -S . -B build-work` failed with *"add_subdirectory given source 'tests'
which is not an existing directory"*. It was restored with `mv` (the tree and all
of its uncommitted modifications were intact; `git status` shows no deletions
again). Anything that configures or builds between 15:19 and the restore used
pre-built binaries — the QML suite still passes 200/8 on them. If the build is ever
behaving oddly, check the Trash before suspecting the sources.
