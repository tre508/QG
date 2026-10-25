---
type: handoff
project: QG
created: 2026-10-20
updated: 2026-10-22
status: active
tags:
  - session-handoff
  - context-recovery
source:
  - GUIDE_STATE.md
  - DEFERRED_TESTS.md
  - .project-memory vault reconciliation
---

# Session Handoff

## Workflow Mode — READ FIRST

**Logic-first, deferred testing** (policy 2026-10-22, [[Agent-Workflow]]).
Implement → static review → document → record the deferred checks → report. Do
**not** build, run CTest/QML tests, launch the app, capture screenshots, or run
benchmarks unless the user authorizes a batch testing phase. Status words:
`IMPLEMENTED` · `STATICALLY REVIEWED` · `NEEDS_REVISION` · `BLOCKED` ·
`TESTING DEFERRED` · `VERIFIED` (only after real execution). Backlog:
`DEFERRED_TESTS.md` → [[Deferred-Test-Register]].

## Current Objective

Two items landed together: **FIX-VIS-013/014** (density presentation) is
IMPLEMENTED / STATICALLY REVIEWED ([[Fix-VIS-013-014-Density]]), and **FIX-UI-008**
(cross-state visual audit) is BLOCKED by the deferred-testing policy because its
deliverable is screenshots ([[Blocked-UI008-Audit]]). Awaiting either the next
implementation task or authorization for the batch testing phase, which would unblock
the audit.

## Last Completed Tasks

1. **FIX-VIS-013/014** — Density + context presentation — [[Fix-VIS-013-014-Density]]
2. **FIX-VIS-011/012** — Card placeholder cues + fixed badge zones — [[Fix-VIS-011-012-Card-Zones]]
2. **FIX-VIS-010** — Video thumbnail imagery (VIS-010) — [[Fix-VIS-010-Thumbnail-Imagery]]
2. **FIX-UI-004** — Standard grid geometry (VIS-008/009) — [[Fix-UI-004-Grid-Geometry]]
3. **FIX-UI-003** — Sidebar icons, device classification & mount states (VIS-005/006/007)
4. **FIX-UI-002** — Path Bar navigation, breadcrumbs & actions (VIS-002/003/004)
5. **FIX-UI-001** — Responsive shell geometry (VIS-001)

## Active Task

**None in flight.** The next task comes from the user; implementation only until a
testing phase is authorized.

### FIX-VIS-010 outcome (for context)

- Root cause: `ThumbnailRole` carried a `QImage` while QML binds it to
  `Image.source` (a `QUrl`) — empty source on `Ready` rows with the placeholder
  hidden, so the well was blank. Now published as `file://` URLs. See
  [[video-thumbnails-not-rendered]].
- Also fixed: freedesktop cache consulted before the extractor; folder rows are no
  longer thumbnail subjects (they wrote bogus `failed` DB rows with `size 0`).
- FIX-UI-004 (grid) remains as recorded: 1593 px workspace → 6 × 244 px, gutters
  24.5/24.5, see [[standard-grid-seven-columns]] / DEC-044.

## Files Modified (this session)

- `qml/QG/PathBar.qml` — density icon/label derived from the active mode, tooltip + a11y
- `qml/QG/strips/ContextStrip.qml` — named density options, active chip, 24 px sizing
- `qml/QG/delegates/VideoCard.qml` — exclusive centre state cue, fixed badge slots,
  22 px TR hover chip, compact Retry, a11y names, meta line anchored to the filename
- `qml/QG/Theme.qml` — `chipScrim` token
- `src/controllers/thumbnail_controller.cpp` — URL publishing, cache-first order,
  folder-row skip
- `qml/QG/delegates/VideoCard.qml` — `objectName: "thumbImage"` (no behaviour change)
- `tests/unit/test_thumb_delivery.cpp` (new), `tests/CMakeLists.txt`,
  `tests/qml/tst_videocard.qml`
- `GUIDE_STATE.md`, `REQUIREMENTS_MATRIX.md`, `DECISIONS.md`, `DEFERRED_TESTS.md`
- Vault: [[Agent-Workflow]], [[Test-Results]], [[Deferred-Test-Register]],
  [[Project-Dashboard]], [[Decision-Index]] (DEC-045), [[Current-State]],
  [[Known-Issues]], [[video-thumbnails-not-rendered]],
  [[Fix-VIS-010-Thumbnail-Imagery]]

## Verification

**Executed before the policy took effect (2026-10-22):**
`qg_thumb_delivery` 10/10 with real MP4+MKV · full QML 203 passed / 4 known
failures · full CTest **47/48 twice** (172.3 s, 181.5 s; only `qg_qml`) · Xvfb real
app: cached frame 320x180 100 % red, painted block 214x125 px at 98.6 % red.
FIX-UI-004 earlier: `tst_grid_geometry` 13/13 at seven widths.

**Static (2026-10-22, FIX-VIS-013/014):** `qmllint` — no new errors (pre-existing
Repeater `Unqualified access` warnings only); no remaining `name: "density"` use.

**Static (2026-10-22, FIX-VIS-011/012):** `qmllint` — no new errors (two `chipScrim`
warnings are artifacts of the stale `build/qml/Theme.qml` copy); zone geometry
checked arithmetically at the 168 px minimum card width.

**Deferred:** nothing has been run since the policy was adopted. Cold/warm cache,
rapid navigation, the seven-width visual sweep and hardware eject validation are
all in [[Deferred-Test-Register]].

## Outstanding Issues

1. **🔴 Worker-pool use-after-free (pre-existing)** — `submitTask` delivers via a
   raw `QObject *receiver` from a pool thread; ~4 % of `qg_keyboard_test` teardown
   runs crash in `QObject::thread()`. Needs its own ASan-backed work item. See
   [[worker-pool-receiver-use-after-free]]
2. **Warm start re-extracts thumbnails** — the QG PNG+DB cache is write-only
   (`decideThumbnail()` / `ThumbnailRepository::lookup()` have no callers)
3. **FIX-DND-003..006 BLOCKED** — need Wayland GUI automation or a human operator
4. **.deb packaging** unvalidated
5. **Probe-cache invalidation** on source change uncovered (THUMB-007 half)
6. **Real eject/unmount** unexecuted (FIX-UI-003 verified capability only; no
   disposable device)
7. **Pre-existing**: `ContentView` reads/assigns an undeclared `focusIndex` in the
   legacy no-`selectionState` branches (dead in production)

## Next Recommended Action

Take the next user-assigned implementation task under the logic-first policy; add
one row per fix to `DEFERRED_TESTS.md`, and run nothing until the batch testing
phase is authorized.
