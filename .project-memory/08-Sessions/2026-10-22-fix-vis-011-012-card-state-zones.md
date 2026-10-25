---
type: session
project: QG
created: 2026-10-22
updated: 2026-10-22
status: implemented
aliases:
  - Fix-VIS-011-012-Card-Zones
tags:
  - session
  - qml
  - delegate
  - visual-fidelity
source:
  - GUIDE_STATE.md
  - DEFERRED_TESTS.md
---

# 2026-10-22 — FIX-VIS-011/012: card placeholder cues and fixed badge zones

## Objective

Distinguishable loading/failed/unreadable placeholders, and fixed badge/action
zones with no overlap.

## What was wrong

The well had two centre occupants (filmstrip + hover play glyph), an ungated play
glyph that painted over failure/denial cues, three default-size hover buttons
overlapping the centre at the minimum card width, a watched badge that moved when
favourite toggled, and a meta line at a hard-coded offset. Details and arithmetic:
[[card-zone-collisions]].

## Changes

| File | Change |
|------|--------|
| `qml/QG/delegates/VideoCard.qml` | `placeholderKind` (from `thumbnailState` 0–4) and `playGlyphVisible` derived properties; exclusive centre cue; amber failure scrim; compact Retry (still `Button` / `thumbRetry`); `thumbUnreadableScrim` named; fixed watched slot; 22 px `ZoneButton` inline component with one chip background for the TR zone; `Accessible.name` on hover actions + Retry; `id: filename` so the meta line anchors to the name |
| `qml/QG/Theme.qml` | `chipScrim` token replaces the duplicated translucent literal |

## Preserved

Focus ring and selection border (outside the well), hover-as-non-selecting, no
destructive action in the hover chip, no new animation (so `Theme.motionEnabled` /
`reducedMotion` behaviour is unchanged), all existing `objectName`s.

## Static validation

- `qmllint` against the generated module: `parent.hovered` warning removed by
  binding to `id: retryButton`; no new errors. Two `chipScrim` warnings are
  artifacts of the stale `build/qml/Theme.qml` copy (token verified in source).
- Zone geometry checked arithmetically at 168 px (95 px well): TR chip 4…26,
  centre glyph 33…61, BR chip 75…91, failed column 21…74 with the TR chip hidden.
- Pre-existing findings left alone (out of scope): `text.left(dot)` in rename
  validation is not a JS method (should be `text.substring(0, dot)`);
  `collisionCheck` var-call style note.

## Deferred

Screenshot matrix (idle / hover / focus / selected / favourite / watched / partial /
failed / denied) at 168, 244 and 360 px card widths; `tst_videocard` re-run; full QML
suite + CTest. Registered in `DEFERRED_TESTS.md` and [[Deferred-Test-Register]].

## Status

**IMPLEMENTED · STATICALLY REVIEWED · TESTING DEFERRED** (policy 2026-10-22,
[[Agent-Workflow]]).
