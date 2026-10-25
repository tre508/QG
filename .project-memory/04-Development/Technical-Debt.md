---
type: development
project: QG
created: 2026-10-20
updated: 2026-10-20
status: active
tags:
  - development
  - technical-debt
  - refactoring
source:
  - ARCHITECTURE_GAP_ANALYSIS.md
  - ARCHITECTURE_REMEDIATION.md
---

# Technical Debt

## Architectural Debt (None Identified)

The implementation faithfully follows DESIGN.md architecture. No architectural shortcuts or incorrect patterns found.

## Implementation Debt

### Visual System

| Item | Location | Effort | Impact |
|------|----------|--------|--------|
| 30+ Unicode glyphs not migrated to Icon.qml | VideoCard, FolderCard, CompactRow, ManifestRow, ReelPanel, PathBar, PlacesSidebar, Main, StatusStrip | Medium | Visual inconsistency; high-contrast mode may not apply correctly |
| `neonOutline` token unwired | Theme.qml, VideoCard, TransportBar, PathBar | Low | High-contrast focus rings fall back to neon (less accessible) |

### Feature Completeness

| Item | Location | Effort | Impact |
|------|----------|--------|--------|
| Recursive search UI not wired | PathBar, SearchController, ContentView | Medium | Subfolders scope disabled; RecursiveSearchService implemented but unused |
| Collections/Playlists scaffold only | DB tables exist; no controller/model/UI | High | Database tables created but no user-facing feature |
| Cross-app clipboard MIME data | ActionsController | Medium | Copy/paste with Nautilus broken |

### Platform Integration

| Item | Location | Effort | Impact |
|------|----------|--------|--------|
| GIO ejector not wired | MountService composition | Low | Eject fails with "not supported" |
| MountService enricher not configured | MountService composition | Low | Device labels, ejectable flags, volumeId not populated |
| Physical drive validation | N/A | Hardware | Disconnect/reconnect UX untested |

## Code Quality Debt

### Minor Inconsistencies

| Item | Location | Description |
|------|----------|-------------|
| PlacesSidebar.qml:98 | `horizontalAlignment: Text.AlignHCenter` on Icon component | Icon has no such property; causes qg_qml/dragdrop test failures |
| `tst_accessibility.qml` | Uses versioned imports (`import QtQuick 2.15`) | Breaks type resolution under Qt 6; should be unversioned |
| `copyPath()` only for access-denied | ActionsController.cpp:299 | Limited use; should be unified with clipboard MIME implementation |

### Test Infrastructure

| Item | Location | Description |
|------|----------|-------------|
| `tst_dragdrop.qml` not created | tests/qml/ | No automated drag/drop contract test |
| `qg_keyboard` flaky segfault | tests/qml/ | Environmental; needs investigation on real display |

## Debt Not Considered Debt (Intentional Design)

| Item | Reason |
|------|--------|
| RecursiveSearchService exists but UI not wired | Architecture extension point per DESIGN.md §20.4; ready when UI prioritized |
| Collections tables exist but no feature | Scaffolded per DESIGN.md §21.2; activation deferred |
| `tst_scaling.qml` only tests `card.scale` | Minimal baseline; extended during FIX-PERF-008 |
| Wayland untested | DESIGN.md §19.3: X11/xcb primary; Wayland-compatible path retained |
| High-DPI untested | No desktop session available; infrastructure ready |

## Debt Paydown Priority

### Quick Wins (< 1 day each)
1. Fix `PlacesSidebar.qml:98` horizontalAlignment on Icon
2. Fix `tst_accessibility.qml` versioned imports
3. Add `onScopeToggled` handler to Main.qml
4. Wire GIO ejector in MountService composition
5. Configure MountService enricher for GIO/UDisks2

### Medium Effort (1-3 days each)
6. Complete icon adoption (30+ glyphs across 9 files)
7. Wire `neonOutline` token for high-contrast focus rings
8. Enable Subfolders scope in PathBar + SearchController
9. Implement system clipboard MIME data (`text/uri-list` + `gnome-copied-files`)

### High Effort (1+ weeks each)
10. Validate .deb packaging end-to-end (clean VM, install, test)
11. Run 50k/100k performance benchmarks (needs hardware)
12. High-DPI/fractional scale verification (needs desktop)
13. Wayland session verification (needs Wayland compositor)

### Requires Product Decision
14. Collections/Playlists UI design (sidebar group vs modal picker)
15. Recursive search UI design (scope chip vs separate panel)

## Debt Prevention Rules

1. **No new Unicode glyphs** — All new icons must use `Icon { name }` pattern
2. **No versioned QML imports** — Use `import QtQuick` not `import QtQuick 2.15`
3. **No direct QML access to filesystem/persistence/playback** — All via controllers
4. **No model mutations from workers** — All via queued signals to GUI thread
5. **No polling for transport state** — Use mpv property observation
6. **No second mpv handle** — Single handle via PlaybackManager singleton
7. **No surface reparenting for Cinema** — Layout state only

## Monitoring

Review this list at each major milestone (end of each FIX-* workstream).
Debt items that persist > 2 workstreams should be escalated to explicit task.