---
type: development
project: QG
created: 2026-10-20
updated: 2026-10-21
status: active
tags:
  - development
  - tasks
  - active
source:
  - GUIDE_STATE.md
  - dev-guide.md
  - ARCHITECTURE_REMEDIATION.md
---

# Active Development Tasks

> **Workflow**: logic-first with deferred testing (2026-10-22). Task statuses below
> mean *implementation* state; runtime validation for anything after that date is
> queued in [[Deferred-Test-Register]], not executed. See [[Agent-Workflow]].

## Priority 1: Visual System (Foundation)

| Task | ID | Status | Description | Dependencies |
|------|----|--------|-------------|--------------|
| Complete monoline icon adoption | FIX-ICON-001..008 | ✅ Complete | 33 monoline SVGs shipped; all target locations migrated to `Icon { name }` (VideoCard, FolderCard, CompactRow, ManifestRow, ReelPanel, PathBar, PlacesSidebar, Main context menus, StatusStrip) | Icon.qml + 33 SVGs shipped; MultiEffect tinting works |
| Wire `neonOutline` for high-contrast | FIX-ICON-009 | ✅ Complete | High-contrast focus/neon outlines bound to `Theme.neonOutline` (HC) / `Theme.focus` (normal) in VideoCard, TransportBar, PathBar | Theme.qml token exists; HC bound to platform |
| Final visual-system compliance audit | FIX-VISUAL-010 | 🔄 In Progress | Code-verified; manual visual validation UNVERIFIED (no desktop session) | All icon/HC wiring complete |
| Density + context presentation | FIX-VIS-013/014 | ✅ Implemented (static review) | Path Bar density button shows the active mode's shipped icon + mode-naming tooltip; strip options named with tooltips/a11y and an active chip. Cycling/persistence screenshots deferred — [[Fix-VIS-013-014-Density]] | FIX-UI-002, FIX-UI-004 |
| Cross-state visual audit | FIX-UI-008 | ⛔ Blocked (policy) | Needs screenshots of Gallery/Compact/Manifest/Stage/Cinema/Reel/dialogs/responsive states; deferred-testing policy forbids captures. Capture matrix ready — [[Blocked-UI008-Audit]] | Testing phase authorization |
| Card placeholder cues + fixed badge zones | FIX-VIS-011/012 | ✅ Implemented (static review) | `placeholderKind` from `thumbnailState`; exclusive centre cue; amber failure scrim + compact Retry; fixed TL slots; 22 px TR chip; meta line anchored to the name. Screenshot matrix deferred — [[card-zone-collisions]] | FIX-UI-004 geometry |
| Video thumbnail imagery | FIX-VIS-010 | ✅ Implemented (static review) | `ThumbnailRole` → `file://` URLs; cache-first lookup; folder rows skipped. Runtime evidence predates the policy ([[video-thumbnails-not-rendered]]) | FIX-UI-004 grid geometry |
| Restore Path Bar nav/breadcrumb/actions | FIX-UI-002 | ✅ Complete | Four-button nav group, breadcrumb root fix, Density + Overflow actions, all 37 SVGs registered; PathBar gate 14/14 | FIX-UI-001 complete |

## Priority 2: Feature Wiring

| Task | ID | Status | Description | Dependencies |
|------|----|--------|-------------|--------------|
| Recursive search ("Subfolders" scope) | FIX-SEARCH-001..007 | ✅ Complete | Subfolders scope wired end-to-end; RecursiveSearchService + RecursiveSearchModel integrated with Manifest view; reactive scope sync; cancellation/generation safety verified | RecursiveSearchService implemented; SearchController scopeRecursive() |
| Implement Collections controller/model/UI | FIX-COLLECTIONS-001 | ⏳ Planned | Activate scaffolded collections tables with CRUD + sidebar integration | DB tables exist; PathRepository + MediaStateRepository provide identity mapping |

## Priority 3: Delivery Validation

| Task | ID | Status | Description | Dependencies |
|------|----|--------|-------------|--------------|
| Validate native .deb build & install | FIX-PACKAGING-001 | ⏳ Planned | Run full packaging pipeline; verify installed app works | CMake install targets; qt_add_qml_module produces installable QML module |

## Priority 4: Performance Verification

| Task | ID | Status | Description | Dependencies |
|------|----|--------|-------------|--------------|
| Run 50k/100k folder benchmarks | FIX-PERF-001..009 | ✅ Complete | Baselines established; first screenful 355–383ms (marginal miss vs 300ms target, documented) | Test infrastructure exists; datasets generated on NVMe SSD |
| High-DPI/fractional scale verification | FIX-PERF-002 | ⏳ Planned | Verify 125%/150%/200% crisp rendering | Qt 6 High-DPI scaling; tst_scaling.qml exists (code path verified) |

## Priority 5: Desktop Integration (FIX-DESKTOP)

| Task | ID | Status | Description | Dependencies |
|------|----|--------|-------------|--------------|
| Cross-app clipboard & DnD (QG ↔ Nautilus) | FIX-DND-001..008 | 🔄 Partially Complete | FIX-DND-001/002/007/008 COMPLETE; FIX-DND-003..006 BLOCKED (no Wayland GUI automation). QClipboard integration applied; QG launches on Wayland. Test fixtures at /tmp/qg_dnd_test/ | QG builds; Wayland session; Nautilus installed |
| System clipboard MIME data | FIX-DESKTOP-004 | 🔄 Code fix applied | QClipboard integration in ActionsController; manual desktop validation blocked | FIX-DND-007 complete |
| Wire GIO ejector | FIX-DESKTOP-005 | ⏳ Planned | Connect MountService::eject to g_drive_eject() | MountService enricher/ejector hooks exist |
| Configure MountService enricher | FIX-DESKTOP-006 | ⏳ Planned | Enable GIO/UDisks2 enrichment for labels, ejectable, volumeId | MountService enricher callback exists |
| Create tst_dragdrop.qml | FIX-DESKTOP-007 | ⏳ Planned | Automated drag/drop contract test | QML contract implemented |
| Cross-app clipboard validation | FIX-DESKTOP-008 | ⏳ Blocked | Manual Nautilus ↔ QG validation | Requires desktop session |
| Drive disconnect/reconnect validation | FIX-DESKTOP-009 | ⏳ Blocked | Physical USB drive test | Requires hardware |

## Task Dependencies Graph

```
FIX-VISUAL-010 (visual compliance)
    │
    └──→ FIX-PACKAGING-001 (packaging)

FIX-DESKTOP-004 (clipboard MIME)
    ├──→ FIX-DESKTOP-008 (cross-app validation) [BLOCKED: needs desktop]
    └──→ FIX-DESKTOP-009 (drive validation) [BLOCKED: needs hardware]

PlacesSidebar.qml:98 (horizontalAlignment on Icon) → unblocks qg_qml + dragdrop tests
```

## Next Actions (Ordered)

1. **FIX-UI-002 COMPLETE** — Path Bar nav/breadcrumb/actions restored (PathBar gate 14/14)
2. **Complete FIX-VISUAL-010** — final visual compliance audit (code-verified; manual visual needs desktop)
2. **Fix PlacesSidebar.qml:98** — remove `horizontalAlignment` on `Icon` to unblock qg_qml/dragdrop test compilation
3. **Validate .deb packaging** — build, install, test on clean VM
4. **Implement clipboard MIME data** — ActionsController copyToClipboard/cut/paste with text/uri-list + gnome-copied-files
5. **Wire GIO ejector + enricher** — MountService composition
6. **Address first-screenful 300ms target** — 355–383ms measured, marginal miss
