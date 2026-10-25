---
type: state
project: QG
created: 2026-10-20
updated: 2026-10-21
status: active
tags:
  - current-state
  - progress
source:
  - GUIDE_STATE.md
  - ARCHITECTURE_CURRENT.md
  - ARCHITECTURE_GAP_ANALYSIS.md
---

# QG Current State

## Implementation Status

### Core Architecture (100% Complete)

All 7 architectural layers implemented with strict separation:

| Layer | Status | Key Components |
|-------|--------|----------------|
| QML Presentation | ✅ | Main, PathBar, PlacesSidebar, ContentView, delegates, Stage, overlays |
| Controllers | ✅ | Browser, Search, Playback, Actions, Mounts, State, Thumbnail, Probe, Keyboard |
| Models | ✅ | FolderModel, SearchProxy, ReelModel, PlacesModel, SelectionState |
| Services | ✅ | Enumerator, Operations, Trash, Mounts, Probe, Thumbnails, Persistence, Sort |
| Workers | ✅ | I/O Pool, CPU/Media Pool, TaskToken, PriorityPool |
| Persistence | ✅ | SQLite WAL, 7 repositories (paths, media_state, thumbnails, recent, bookmarks, settings, collections) |
| Playback | ✅ | MpvBackend, VideoSurface, PlaybackManager, PlaybackSession, PlaybackContext |

### Test Results (46/47 PASS — 98%)

**Command**: `QT_QPA_PLATFORM=offscreen TMPDIR=/tmp ctest --test-dir build`  
**Duration**: ~130 seconds  
**Failures**: `qg_qml` only — 4 pre-existing subtest failures (keyboard focus, duplicate Alt+Left, TransportBar icon state), not app logic.  
**PathBar gate**: `QT_QPA_PLATFORM=offscreen build/tests/qg_qml_test -input tests/qml/tst_pathbar.qml` — 14 passed, 0 failed (FIX-UI-002).
**Grid geometry gate**: `QT_QPA_PLATFORM=offscreen build/tests/qg_qml_test -input tests/qml/tst_grid_geometry.qml` — 13/13 (FIX-UI-004); measured at seven window widths, computed columns == laid-out columns and gutters equal within 1.5 px.

### Known Gaps

| Gap | Severity | Location |
|-----|----------|----------|
| Sidebar width / responsive geometry (VIS-001) | ✅ Resolved | Main.qml — PathBar + ContextStrip moved into workspace column beside sidebar; sidebar width now responsive (panel 240 / rail 56 / hidden per DESIGN.md §3.5 breakpoints); ContextStrip availableWidth uses workspace width |
| Path Bar navigation group (VIS-002) | ✅ Resolved | PathBar.qml — back/forward/parent/refresh restored with correct themed icons (parent/refresh), tooltips, accessible names, focus overlays; Parent disabled at root; all 37 monoline SVGs registered in qrc (was 16) so icons no longer fall back to dots |
| Breadcrumb root slash (VIS-003) | ✅ Resolved | PathBar.qml — separator changed from "/" to "›" rendered only between segments; root "/" appears exactly once |
||| Search/Order/Density/Overflow cluster (VIS-004) | ✅ Resolved | PathBar.qml — Density button + menu (Gallery/Standard/Compact) and Overflow button + menu (Show hidden files) implemented (were empty placeholder slots); Order icon fixed to `sort`; wired to browserController.setDensityValue / setShowHidden ||
||| Standard grid cards too narrow (VIS-008) | ✅ Resolved | ContentView.qml — column count now derives from the §3.5 density target instead of `min_card`; 1593 px workspace gives 6 × 244 px (was 8 × 179) |
||| Standard grid left-anchored / dead right band (VIS-009) | ✅ Resolved | ContentView.qml — `rightMargin = leftMargin` shrank the flow so GridView laid out 7 of 8 columns; now `leftMargin = gridGutter`, `rightMargin = 0`, measured gutters 24.5 / 24.5. See [[standard-grid-seven-columns]] |
|| Inline video rendering (PLAY-VIS-001/002/003/006) | ✅ Resolved | video_surface.cpp — VideoSurfaceItem now QQuickFramebufferObject (item-sized FBO); mpv frames render inside Stage, purple artifact eliminated, duration/position propagate ||
| VIS-013/014 — density presentation | ✅ Implemented (static review) 2026-10-22 | Path Bar density button derives icon+label from the active mode (was a static generic grid identical to Gallery); strip options named, active chip, fit the 28 px strip. Screenshots deferred. See [[Fix-VIS-013-014-Density]] |
| FIX-UI-008 — cross-state visual audit | ⛔ Blocked (policy) | Requires runtime captures; deferred-testing policy forbids them until authorized. No findings recorded. See [[Blocked-UI008-Audit]] |
| VIS-011/012 — card placeholders & badge zones | ✅ Implemented (static review) 2026-10-22 | Centre zone is now exclusive (`playGlyphVisible`), cues derive from `thumbnailState`, TR hover chip is 22 px in one chip background, watched badge holds a fixed slot, meta line anchors to the filename. Runtime screenshots deferred. See [[card-zone-collisions]] |
| VIS-010 — thumbnails never rendered | ✅ Resolved 2026-10-22 | Model exposed a `QImage` to `Image.source` (a `QUrl`) → empty source on `Ready` rows. Thumbnails now ship as `file://` URLs published by the controller; verified end-to-end under Xvfb with a colour-coded clip (98.6 % of the painted block matched the expected frame colour). See [[video-thumbnails-not-rendered]] |
| Worker-pool delivery use-after-free (intermittent SEGFAULT) | 🔴 Open (pre-existing) | `src/workers/tasks.cpp:81` invokes queued delivery on a raw `QObject *receiver` from a pool thread; ~4 % of `qg_keyboard_test` teardown runs crash in `QObject::thread()`. Not caused by FIX-UI-004; see [[worker-pool-receiver-use-after-free]] |
| System clipboard MIME data | 🟡 Code fix applied, manual blocked | ActionsController (`copyToClipboard`/`cut`/`paste` now integrate QClipboard with `text/uri-list` + `x-special/gnome-copied-files`); real-desktop Ctrl+C/V validation BLOCKED (no Wayland GUI automation) |
| GIO ejector not wired | 🟡 Medium | MountService::eject (`m_ejector` callback not connected in composition) |
| MountService enricher not configured | 🟡 Medium | MountService (GIO/UDisks2 enrichment for labels, ejectable, volumeId not active) |
| .deb packaging unvalidated | 🟡 Delivery | packaging/debian/ |
| High-DPI/fractional scale untested (visual) | ⚪ Platform | No desktop session; tst_scaling.qml covers code path only |
| Wayland untested | ⚪ Platform | Render-API path retained; no Wayland session |
| Cross-app clipboard/DnD validation | ⚪ Blocked | FIX-DND-003..006 blocked — no Wayland GUI automation (ydotool/wtype); code fix applied in FIX-DND-007 |
| Drive disconnect/reconnect validation | ⚪ Blocked | No physical USB drive |

### Key Decisions Implemented

- **DEC-007**: Single mpv handle owned by MpvBackend
- **DEC-008**: One active playback session
- **DEC-009**: mpv_render_context + Qt Quick scene-graph
- **DEC-010**: In-window Cinema (no new window)
- **DEC-014**: Authoritative ordered folder/playback context
- **DEC-028**: Kernel-first mount discovery with enricher/ejector hooks
- **DEC-043**: CoreServices single ownership root
- **Runtime**: LC_NUMERIC=C before mpv_create()
- **Runtime**: Defer loadfile until render context ready

### Active Development Focus

**Priority 1**: FIX-UI-001..004 complete (shell geometry, Path Bar, sidebar icons/mount states, Standard grid geometry). Next: FIX-VISUAL-010 final visual-system compliance audit  
**Priority 2**: Validate native .deb build & install  
**Priority 3**: System clipboard MIME data (`text/uri-list` + `gnome-copied-files`)  
**Priority 4**: First-screenful < 300ms target (355–383ms measured; marginal miss documented)  
**Priority 5**: Platform verification (Wayland, High-DPI visual, cross-app DnD) — blocked without desktop session

### Blockers

- No physical USB drive for drive disconnect/reconnect validation
- No desktop session for cross-app clipboard/DnD validation
- No Wayland session for platform verification
