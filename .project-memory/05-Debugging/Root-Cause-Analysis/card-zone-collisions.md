---
type: root-cause-analysis
project: QG
created: 2026-10-22
updated: 2026-10-22
status: resolved
aliases:
  - VIS-011-012
tags:
  - qml
  - delegate
  - layout
  - badges
  - placeholder
source:
  - FIX-VIS-011/012
  - qml/QG/delegates/VideoCard.qml
---

# Root Cause: Card State Cues and Hover Controls Collided (VIS-011/012)

Related: [[Current-State]] · [[Known-Issues]] · [[QML-Conventions]] ·
[[Fix-VIS-011-012-Card-Zones]]

## Symptoms

Blank thumbnail wells had no dependable state marker, and a hovered card's controls
appeared scattered / on top of one another.

## Findings (source inspection, no runtime needed)

1. **Two centre occupants.** `thumbFilmstrip` (visible whenever the row is not
   `Ready`) and `hoverPlayGlyph` (visible whenever hovered) both used
   `anchors.centerIn: parent`. Hovering a missing/loading card painted the play
   triangle over the filmstrip.
2. **No state gate on the play glyph.** It was declared after the failed column and
   the lock icon, so hovering a failed card drew a play triangle across the amber
   warn glyph and the Retry control.
3. **Default-sized hover buttons.** Three bare `ToolButton`s (~36–40 px each) at a
   4 px TR margin occupy y 4…44 in a 95 px well at the minimum card width (168 px);
   the centred 28 px glyph occupies y 33…61 — an 11 px vertical overlap, plus a
   horizontal overlap once the row is ~110 px wide.
4. **Watched badge relocated on favourite toggle** (`topMargin: favorite ? 20 : space1`):
   markers swapped slots instead of holding fixed ones.
5. **Meta line at a hard-coded offset** (`well.bottom + scale(20)`) — the filename
   alone needs ≈20.8 px at scale 1, so the two lines could touch and would overlap
   under font scaling.

## Fix

Zone exclusivity by construction: one derived boolean (`playGlyphVisible`) decides
who owns the centre, `placeholderKind` derives the cue from the model's
`thumbnailState` 0–4, the TR chip became three 22 px buttons inside one chip
background, the watched badge got a fixed slot, and the meta line anchors to the
filename. See [[Fix-VIS-011-012-Card-Zones]].

## Verification

Static only (deferred-testing policy in force): `qmllint` (no new errors; two
`chipScrim` warnings are artifacts of the stale `build/qml/Theme.qml` copy) and
arithmetic zone checks at the 168 px minimum card width. Runtime screenshots are
queued in the deferred register.

## Lesson

Overlay collisions in a delegate are a **z-order + geometry + visibility** problem,
not a styling problem. Derive one occupancy property per zone instead of writing
independent `visible:` expressions; then overlap becomes impossible to express.
