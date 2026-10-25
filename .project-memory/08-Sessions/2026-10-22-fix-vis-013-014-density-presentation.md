---
type: session
project: QG
created: 2026-10-22
updated: 2026-10-22
status: implemented
aliases:
  - Fix-VIS-013-014-Density
tags:
  - session
  - qml
  - pathbar
  - density
  - visual-fidelity
source:
  - GUIDE_STATE.md
  - DEFERRED_TESTS.md
---

# 2026-10-22 — FIX-VIS-013/014: density and context presentation

## Objective

Make the active view density discernible and give it an unambiguous control.

## What was wrong

`PathBar.qml` hard-coded `Icon { name: "density" }` in the Density button — a
generic 2×2 grid, visually identical to `density-gallery.svg`, invariant across
Gallery/Standard/Compact. Tooltip and accessible name were the bare word "Density".
The **ContextStrip** had the three-icon segmented control, but its options had no
names, the active state was a text-colour shift only, and default `ToolButton`
padding made each option 36 px tall inside a 28 px strip.

## Changes

| File | Change |
|------|--------|
| `qml/QG/PathBar.qml` | `densityIconName` + `densityLabel` derived from the same `density` value the menu checks; button icon, tooltip ("Density: Standard") and `Accessible.name` use them; menu and Order menu unchanged |
| `qml/QG/strips/ContextStrip.qml` | Options carry `label` + `shortcut` → tooltip ("Standard (Ctrl+2)") and `Accessible.name`; `Accessible.checkable/checked`; active option gets a filled chip + hairline border; `padding: 0`, 24 px implicit size so options fit the strip |

## Preserved

One authoritative value (`browserController.density`) with one write path
(`setDensityValue`), `Ctrl+1/2/3` routing, per-folder persistence, `ContentView`
density modes, the Order menu in the Path Bar. No new state was introduced.

## Static validation

`qmllint`: no new errors; remaining `Unqualified access` warnings are the
pre-existing Repeater/`modelData` pattern in these files. Verified no remaining
`name: "density"` use. `Theme.hairlineStrong` confirmed present.

## Deferred

Cycle all three densities and capture each with active state and hover tooltip;
confirm Path Bar icon and strip selection agree; `Ctrl+1/2/3`; folder revisit
persistence; `tst_pathbar`, `tst_contextstrip`, `tst_density`, `tst_main`.

## Status

**IMPLEMENTED · STATICALLY REVIEWED · TESTING DEFERRED** ([[Agent-Workflow]]).
