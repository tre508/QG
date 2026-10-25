---
type: root-cause-analysis
project: QG
created: 2026-10-21
updated: 2026-10-21
status: resolved
tags:
  - grid
  - layout
  - gridview
  - qml
  - responsive
source:
  - FIX-UI-004
  - DESIGN.md §3.5, §5.4
  - GUIDE_STATE.md
---

# Root Cause: Standard Grid Showed Seven Narrow Columns With a Dead Right Band

Related: [[Current-State]] · [[Known-Issues]] · [[Decision-Index]] · [[Fix-UI-004-Grid-Geometry]]

## Observed Failure

At the wide reference window (1833 × 1080) the Standard density grid displayed
**about seven narrow cards** pinned to the left, with a large unused band on the
right. Cards were ~179 px wide against a documented ~240 px target.

## Two Independent Defects

### 1. Column count derived from `min_card` (cards too narrow)

`ContentView` computed:

    columns = max(1, floor((width - 2*pad) / (minCard + gap)))      // minCard = 168

That maximises the column count instead of aiming at the design target, so the
grid chose the narrowest legal card everywhere. DESIGN §3.5 gives both a
`min_card` floor **and** a per-band *target* card width (240 at the wide layout),
and §5.4 calls Standard "3–6 columns typical".

Measured before the fix, 1593 px workspace: `columns = 8`, `cardWidth = 179`.

### 2. `rightMargin` shrank the flow below the cell grid (left-anchored, dead band)

The GridView set `leftMargin` **and** `rightMargin` to the centring gutter:

    leftMargin  = max(pad, (width - occupied) / 2)
    rightMargin = leftMargin

Flickable margins reduce the flow width, so the usable flow became
`1593 − 2·24.5 = 1544 px`, while eight cells needed `8 × 195 = 1560 px`. GridView
therefore fitted **7 columns**, not 8 — while `contentView.columns` still
reported 8 and the gutter maths assumed 8. The flow was pinned at `leftMargin`,
leaving 24.5 px on the left and a ~219 px dead band on the right
(1593 − 24.5 − (6·195 + 179) = 219.5).

The diagnostic signature: **the computed column count and the number of cards
actually laid out disagreed**, and `grid.itemAtIndex(columns - 1)` landed on the
second row instead of the end of the first.

## Fix

- Derive `columns` from the density target: the largest `n` whose cards still
  reach the target, clamped to `[1, maxColumns(min_card)]`.
- Clamp `cardWidth` into `[min_card, min(max_card, available)]`; the ceiling may
  clamp (that surplus becomes centred gutters, per §3.5 — never stretched cards).
- Centre with a **single** `leftMargin = (width - occupied) / 2` and
  `rightMargin = 0`, so the flow can always hold every computed column.
- Supply the §3.5 band target from the shell (`Main.qml`), keeping window-width
  bands out of the view.

## Verification

`tests/qml/tst_grid_geometry.qml` (13/13) measures the live GridView at seven
window widths and asserts: computed columns == laid-out columns, gutters equal
within 1.5 px, no card crosses the viewport edge, 16:9 thumbnail region, constant
44 px metadata band, degenerate widths stay valid, 320 px ceiling clamps and
centres, the shell target changes the count, Gallery stays larger/sparser, focus
survives reflow, and 1000 rows instantiate < 200 delegates.

Reference viewport after the fix: **6 columns × 244 px, gutters 24.5 / 24.5**.

## Lesson

When a virtualised view's *computed* geometry and its *actual* layout can
disagree, assert on the rendered result (delegate positions) rather than on the
formula. A non-zero Flickable `rightMargin` consumes flow width that the last
cell still needs.
