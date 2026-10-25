---
type: blocked-task
project: QG
created: 2026-10-22
updated: 2026-10-22
status: blocked
tags:
  - audit
  - visual-regression
  - blocked
  - testing
source:
  - GUIDE_STATE.md
  - DEFERRED_TESTS.md
  - Agent-Workflow.md
---

# FIX-UI-008 — Cross-State Visual Regression Audit (BLOCKED)

## Why blocked

The audit produces findings **from screenshots** of Gallery, Compact, Manifest,
Stage, Cinema, Reel, dialogs and responsive states. The active workflow policy
([[Agent-Workflow]], 2026-10-22) forbids app launches, screenshot capture and
visual regression passes until the user authorizes a testing phase. This is a
policy block, not a missing dependency and not a code defect.

## Rule being honoured

The task states findings may be added "only after observation". Nothing was
observed, so **no findings have been recorded** — none were invented or inferred
from source reading.

## Capture matrix (ready to execute)

| Group | States |
|-------|--------|
| Browse | Gallery · Standard · Compact × idle · hover · focus · selected |
| Delegates | FolderCard (hover, drop target) · CompactRow · ManifestRow |
| Playback | Stage idle · Stage playing · Cinema layout · Reel open · Reel filtered |
| Overlays | StateOverlay: empty · denied · error · no-results · ToastHost with Undo |
| Dialogs | Properties · RenameConflict · ConfirmDelete |
| Responsive | 1100 · 900 · 820 · 700 px (rail / overlay behaviour) |

## Fixtures

One temp folder with: four videos whose thumbnails extract, one truncated video
(extraction failure), one `chmod 000` video (permissions), two subfolders, and
200+ files for scroll/virtualization.

## Audit method

Launch under Xvfb at 1833×1080 with an isolated HOME/XDG root (the recipe proven
during VIS-010), capture each state, then check each capture against DESIGN §§5–13
and §§25.2–25.10, recording one PASS/FAIL row per clause with the capture filename
as evidence.

## Unblock condition

The user authorizes the batch testing phase (or explicitly permits screenshots for
this audit).
