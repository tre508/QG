---
type: session
project: QG
aliases:
  - Fix-UI-004-Grid-Geometry
created: 2026-10-21
updated: 2026-10-21
status: completed
tags:
  - session
  - fix-ui-004
  - grid
  - layout
  - vis-008
  - vis-009
source:
  - GUIDE_STATE.md
  - DECISIONS.md
---

# Session 2026-10-21 — FIX-UI-004 Standard Grid Geometry

Work item: **FIX-UI-004 — Recompute and Centre Standard Grid** (VIS-008 / VIS-009).
Prerequisite **FIX-UI-001** verified COMPLETE (shell geometry, sidebar panel/rail/hidden).

Related: [[Current-State]] · [[Session-Handoff]] · [[Known-Issues]] ·
[[Completed-Tasks]] · [[Decision-Index]] ·
[[standard-grid-seven-columns|Root cause: seven narrow columns]]

## What was wrong

| Discrepancy | Cause |
|---|---|
| VIS-008 — cards too narrow | `columns` was derived from `min_card` (168), maximising columns; the reference 1593 px workspace produced **8 × 179 px** instead of ~6 × 244 |
| VIS-009 — left-anchored, dead right band | `GridView.rightMargin` equalled `leftMargin`, shrinking the flow to 1544 px while 8 cells needed 1560 → GridView laid out **7** columns and pinned the flow left (~219 px dead right) |

## What changed

- `qml/QG/ContentView.qml` — one authoritative geometry block: `targetCardWidth`
  (shell input), `targetCard`, `availableWidth`, `cardWidthAt(n)`, `maxColumns`,
  `columns`, `cardWidth`, `occupiedWidth`, `gridGutter`, `metaBandHeight`.
  GridView: `leftMargin: gridGutter`, `rightMargin: 0`.
- `qml/QG/Main.qml` — `gridTargetCardWidth` implements the §3.5 bands
  (240/220/200/190) and is bound into the view.
- `tests/qml/tst_grid_geometry.qml` — new, 11 measured test functions (13/13 PASS
  including init/cleanup).
- `tests/qml/tst_contentview.qml` — column expectations updated from the old
  `min_card`-driven values to the target-driven ones.

## Measured result (Standard, after the fix)

| Window | Workspace | Columns (computed = laid out) | Card | Gutters L/R |
|---|---|---|---|---|
| 1833 | 1593 | 6 = 6 | 244 | 24.5 / 24.5 |
| 1500 | 1260 | 4 = 4 | 291 | 24 / 24 |
| 1150 | 1094 | 4 = 4 | 249 | 25 / 25 |
| 1100 | 1044 | 4 = 4 | 237 | 24 / 24 |
| 900 | 844 | 3 = 3 | 254 | 25 / 25 |
| 820 | 764 | 3 = 3 | 228 | 24 / 24 |
| 700 | 700 | 3 = 3 | 206 | 25 / 25 |

Gallery: 1593 px → 4 × 360 (ceiling clamp), gutters 52.5 / 52.5.

## Verification

- `tst_grid_geometry` 13/13; `tst_density` 5/5; `tst_scaling` 22/22;
  `tst_main` 4/4; `tst_virtualization` 4/4.
- `tst_contentview` 5/6 — the failure is the documented pre-existing
  `test_focusAndSelectionAreDistinct` (mock `selectedCount()`), not geometry.
- Screenshot comparison **UNVERIFIED** — no graphical session; validation is
  measured geometry, not visual judgement.

## Follow-up (not done here)

- `ContentView` reads/assigns an undeclared `focusIndex` in the legacy
  no-`selectionState` branches (pre-existing; production always injects
  `selectionState`).
- Visual capture at the seven widths remains blocked without a desktop session.
