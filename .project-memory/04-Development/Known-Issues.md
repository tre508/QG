---
type: development
project: QG
created: 2026-10-20
updated: 2026-10-21
status: active
tags:
  - development
  - issues
  - bugs
source:
  - GUIDE_STATE.md
  - ARCHITECTURE_GAP_ANALYSIS.md
  - FIX_DESKTOP_001_AUDIT.md
---

# Known Issues

## Test Infrastructure Failures (Non-App-Logic)

| Issue | Test | Status | Root Cause | Fix |
|-------|------|--------|------------|-----|
| `PlacesSidebar.qml:98` `horizontalAlignment` on `Icon` | `qg_qml`, `dragdrop` | FAIL | `Icon` component has no `horizontalAlignment` property; blocks QML test compilation entirely | Remove or replace `horizontalAlignment: Text.AlignHCenter` on Icon at PlacesSidebar.qml:98 |
| Missing `onScopeToggled` handler | `qg_qml`, `dragdrop` | FAIL | PathBar emits `onScopeToggled` but Main.qml PathBar connections block lacks handler | Add `onScopeToggled: function(subfolders) { if (searchController) searchController.setScope(subfolders) }` to Main.qml PathBar connections |
| `qg_keyboard` flaky segfault | `qg_keyboard` | FLAKY (passed this run) | Environmental (headless CI / offscreen platform), not code-related | Investigate on real display |

## Visual Consistency

| Issue | Component | Status | Description |
|-------|-----------|--------|-------------|
| Icon glyph migration | VideoCard, FolderCard, CompactRow, ManifestRow, ReelPanel, PathBar, PlacesSidebar, Main, StatusStrip | ✅ Resolved | 33 monoline SVGs shipped; all target locations migrated to `Icon { name }`; only `·` and `…` text separators remain |
| `neonOutline` token wiring | Theme.qml, VideoCard, TransportBar, PathBar | ✅ Resolved | High-contrast focus/neon outlines bound to `Theme.neonOutline` (HC) / `Theme.focus` (normal) |
| Inline video rendering (PLAY-VIS-001/002/003/006) | video_surface.cpp | ✅ Resolved | VideoSurfaceItem is now QQuickFramebufferObject (item-sized FBO); mpv frames render inside Stage; purple artifact eliminated; duration/position propagate |
| Path Bar navigation group (VIS-002) | PathBar.qml | ✅ Resolved | back/forward/parent/refresh restored with correct themed icons, tooltips, accessible names, focus overlays; Parent disabled at root; all 37 monoline SVGs registered in qrc (was 16) |
| Breadcrumb root slash (VIS-003) | PathBar.qml | ✅ Resolved | separator changed from "/" to "›" between segments; root "/" appears exactly once |
| Search/Order/Density/Overflow cluster (VIS-004) | PathBar.qml | ✅ Resolved | Density + Overflow buttons/menus implemented (were empty placeholder slots); Order icon fixed to `sort`; wired to setDensityValue / setShowHidden |
| Standard grid cards too narrow (VIS-008) | ContentView.qml | ✅ Resolved (FIX-UI-004) | Column count derived from `min_card` (168) maximised columns → 8×179 px at the 1593 px reference workspace. Now derived from the §3.5 band target: 6×244 px. See [[standard-grid-seven-columns]] |
| Standard grid left-anchored / dead right band (VIS-009) | ContentView.qml | ✅ Resolved (FIX-UI-004) | `rightMargin = leftMargin` shrank the flow to 1544 px while 8 cells needed 1560 → 7 of 8 columns laid out, pinned left (~219 px dead right). Now `leftMargin = gridGutter`, `rightMargin = 0`; measured gutters 24.5/24.5 |

## Environment

| Issue | Status | Description |
|-------|--------|-------------|
| Startup abort: `xsettings`/`antialiasing` | ✅ Resolved (shim) | g-s-d 50 moved `antialiasing`/`hinting`/`rgba-order` to `…xsettings.deprecated`; a desktop-theme consumer still reads the old schema and GLib aborts. QG has no GSettings code. Fix: `scripts/fix-xsettings-compat.sh` (user-level schema shim, no root, `--remove` to undo). **Verified 2026-10-22:** after installing, `DISPLAY=:0 timeout 12 ./build/src/qg` stayed up (exit 124, no abort) and reached `QG starting`. See [[startup-glib-gio-settings-crash]] |

## Thumbnails

| Issue | Status | Description |
|-------|--------|-------------|
| VIS-010 — ready rows painted no imagery | ✅ Resolved 2026-10-22 | `ThumbnailRole` carried a `QImage`; `Image.source` needs a `QUrl`, so the source was empty while the row claimed `Ready` and hid its placeholder. Now published as `file://` URLs. See [[video-thumbnails-not-rendered]] |
| Warm start re-extracts every thumbnail | 🟡 Open (pre-existing) | The QG PNG + DB cache is write-only: `decideThumbnail()` and `ThumbnailRepository::lookup()` have **no callers**, so a warm start pays FFmpeg for every visible row instead of serving the stored PNG (DESIGN §18.2 "Serve"). |
| Probe cache not invalidated on source change | 🟡 Open | THUMB-007 is only half covered: thumbnail invalidation is verified, probe invalidation is not. |

## Concurrency

| Issue | Component | Status | Description |
|-------|-----------|--------|-------------|
| Intermittent SEGFAULT in worker-pool delivery | `src/workers/tasks.cpp:81` (`submitTask`) | 🔴 Open (pre-existing) | Queued delivery invokes a **raw `QObject *receiver`** from a pool thread; if the receiver is destroyed while the task is queued, the worker crashes in `QObject::thread()`. Reproduces ~4 % sequentially (1/25 `qg_keyboard_test`, 1/14 under gdb) — previously mis-recorded as "environmental". See [[worker-pool-receiver-use-after-free]] |

## Cross-App Desktop Integration (UNVERIFIED)

| Issue | Component | Status | Description |
|-------|-----------|--------|-------------|
| Sidebar width / responsive geometry (VIS-001) | Main.qml | ✅ Resolved | PathBar + ContextStrip moved into workspace column beside sidebar; sidebar width now responsive (panel 240 / rail 56 / hidden per DESIGN.md §3.5 breakpoints); ContextStrip availableWidth uses workspace width |
| System clipboard MIME data | ActionsController | 🟡 Code fix applied, manual blocked | QClipboard integration added: `copyToClipboard()`/`cut()` write `text/uri-list` + `x-special/gnome-copied-files`; `paste()` reads QClipboard. Real-desktop Ctrl+C/V validation BLOCKED (no Wayland GUI automation). |
| Cut intent not communicated | ActionsController | 🟡 Medium | `x-special/gnome-copied-files` with "cut" marker now set on cut; not yet manually verified with Nautilus |
| GIO ejector not wired | MountService::eject/unmount | ✅ Resolved (FIX-UI-003) | Real operations via `gio mount -u/-e` on the I/O pool when no injected ejector; system mounts refused; capability-gated |
| MountService enricher not configured | MountService | ✅ Resolved (FIX-UI-003) | Kernel (`/sys`, `/dev/disk/by-label`) + GVfs/UDisks2 (`gio mount -l -i`) enrichment active; loopback/plumbing mounts classified and excluded from the sidebar |
| Physical drive validation | All | 🟡 Partial (FIX-UI-003) | Mount appear/disappear/reconnect covered by fixtures (`disconnectedVolumeRetainedAndReconnected`, `disconnectedVolumeAgesOut`); live classification verified on 3 USB-attached drives; real *eject* still unexecuted (no authorized disposable device) |
| Cross-app clipboard/DnD validation | All | ⚪ Blocked | FIX-DND-003..006 blocked — no Wayland GUI automation; code fix applied |

## Platform Verification (UNVERIFIED)

| Issue | Status | Description |
|-------|--------|-------------|
| High-DPI/fractional scale (125%/150%/200%) | ⚪ Unverified | `tst_scaling.qml` only transforms `card.scale`; no real compositor test |
| Wayland session | ⚪ Unverified | Render-API path retained but untested on Wayland |
| Hardware decoding (VDPAU/VA-API) | ⚪ Unverified | `qg_resume_playback` fails in headless CI; works on real GPU? |
| Native file dialog integration | ⚪ Unverified | Uses portal/WM where available; fallback graceful |
| Screen reader / AT-SPI | ⚪ Unverified | `Accessible.name/role` on cards; no ORCA validation |

## Feature Gaps

| Feature | Status | Description |
|---------|--------|-------------|
| Recursive search UI | ✅ Complete | Subfolders scope wired end-to-end; RecursiveSearchService + RecursiveSearchModel integrated with Manifest view |
| Collections/Playlists | 🟡 Planned | DB tables exist (`collections`, `collection_items`); no controller/model/UI |
| Native .deb packaging validation | 🟡 Planned | Metadata exists; no successful build/install/test cycle |

## Performance (Not Yet Measured Visually)

| Target | Status | Evidence |
|--------|--------|----------|
| First screenful < 300ms | ⚠️ Marginal miss | 355–383ms measured (GUIDE_STATE.md FIX-PERF-009) |
| 60 FPS scrolling | ⚪ Unverified | Requires real display |
| No GUI-thread disk I/O | ✅ Verified | All I/O on worker pools |
| Rapid nav (20 folders) → final only | ✅ Verified | `rapidNavigationCancelsStaleResults` PASS |
| Bulk move 1000 files → no thumb starvation | ✅ Verified | `bulkOperationsDontStarveVisible` PASS |
| Peak memory (100k entries) | ✅ Verified | `test_probe_memory` PASS |

## Accessibility

| Issue | Status | Description |
|-------|--------|-------------|
| Monochrome/color-blind rendering | ⚪ Unverified | Design claims states distinguishable by shape+color; no automated verification |
| Reduced motion platform signal | 🟡 Partial | Qt 6.10 has no `reducedAnimation` signal; app-toggle authoritative |

## Issue Prioritization

### 🔴 Critical (Block Cross-App Integration)
1. System clipboard MIME data in ActionsController

### 🟡 High (Visual/Feature Polish)
2. Validate .deb packaging
5. First-screenful 300ms target (marginal miss)

### ⚪ Blocked (Need Hardware / Desktop)
6. Cross-app clipboard/DnD validation
7. Real eject/unmount execution on a disposable device (FIX-UI-003 verified capability + fixture hotplug only)
8. Wayland verification
9. High-DPI visual verification
