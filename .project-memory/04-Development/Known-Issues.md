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
| Selection/focus never reach the view (AUD-004) | SelectionState, ContentView, Main, all four delegates | ✅ Resolved (FIX-SEL-01, 2026-10-25) | `SelectionState` exposed only invokable getters (no `Q_PROPERTY`), so every QML binding that read it was evaluated once at creation: no selection border, no cyan focus ring, status count/bytes frozen at 0, "N items" menu title frozen at 1, Ctrl+A and Shift-range inert, and `clear()` also destroyed the cursor. Now properties + `selectionRevision`; range/select-all take the view's identity list; `indexOfIdentity` is Q_INVOKABLE (byte total) and `SearchProxy`/`RecursiveSearchModel` expose `identityAt` (Manifest rows). See [[2026-10-25-fix-aud-004-selection-focus]] |
| Stage drawn over the grid (AUD-009) | Main.qml, ContentView.qml | ✅ Resolved for the reflow (FIX-STAGE-01, 2026-10-25; re-verified 2026-10-26 — no code change needed) | The Stage was a sibling anchored over the grid and `ContentView` (topMargin = pad only) knew nothing about it, so the first rows were covered and the cursor could reach invisible cards. `ContentView` now publishes `reservedTop`/`topInset` applied to all three presentations and `Main.qml` binds it to `stage.height + Theme.space3`. While fixing this, the visible-range reporter's margin subtraction was found to drop the first screen entirely at large insets (no thumbnail requests while playing) and corrected. **Gate gap:** the acceptance harness lives outside the repo, so `ctest` does not cover the inset. The Stage is still top-anchored rather than at the playing item's position. See [[2026-10-25-fix-aud-009-stage-reflow]] |
| Search could never enter the Manifest intensity (AUD-005) | SearchController, Main.qml, PathBar.qml, ContentView.qml | ✅ Fixed (FIX-SRCH-01, 2026-10-26; static only) | Two independent causes: `SearchController` declared **no `Q_PROPERTY`**, so `Main.qml`'s `manifestActive` read (`searchController.active === true`) was `undefined` → permanently false and the chrome never switched; and `setFolderName`/`setSortLabel` were plain members, so the QML call raised a TypeError inside `onActiveChanged` and aborted the handler before `enterManifest()`. Now `active`/`summary`/`resultCount` are notifying properties (member set before notify, de-duplicated; `clear()` re-notifies `resultsChanged` so a bound summary cannot stay stale after Esc), both setters are `Q_INVOKABLE`, and the shell reads the summary/applied query from the properties. **Deferred:** the whole §7.2/§25.4 presentation path now becomes reachable for the first time, so these are unproven — (1) focus ownership between the field and `manifestList` (`ContentView.qml:867`), (2) §7.3 ↑/↓ row focus (no `Keys` contract on the manifest list), (3) AUD-023's selection restore. See [[2026-10-26-fix-aud-005-search-manifest]], `DEFERRED_TESTS.md` FIX-SRCH-01 |
| Persisted favorite/watched/progress never applied after the first navigation (AUD-006) | `src/controllers/state_controller.{hpp,cpp}` | ✅ Fixed (FIX-AUD-006, 2026-10-26; static only) | `loadIntoModel()` ran once in `CoreServices::wire()` (`src/core/composition.cpp:98`) before the first `navigate()` populated the model, so the decoration loop never executed, and every later `applySnapshot` → `modelReset` replaced the rows without re-applying state — hearts, violet checks and progress bars stayed empty for the whole session. Now `setModel()` subscribes to `modelReset`/`rowsInserted`, clears `m_applied` on reset, and `scheduleLoad()` applies persisted state to new rows exactly once per identity per generation on a coalesced zero-delay timer. `loadIntoModel()` remains the immediate entry point for tests. **Deferred:** first-load / navigation / refresh restore, restart persistence, and first-paint on a 5,000-entry folder. See [[2026-10-26-fix-aud-006-persisted-state]], `DEFERRED_TESTS.md` FIX-AUD-006 |

## Environment

| Issue | Status | Description |
|-------|--------|-------------|
| Startup abort: `xsettings`/`antialiasing` | ✅ Resolved (shim) | g-s-d 50 moved `antialiasing`/`hinting`/`rgba-order` to `…xsettings.deprecated`; a desktop-theme consumer still reads the old schema and GLib aborts. QG has no GSettings code. Fix: `scripts/fix-xsettings-compat.sh` (user-level schema shim, no root, `--remove` to undo). **Verified 2026-10-22:** after installing, `DISPLAY=:0 timeout 12 ./build/src/qg` stayed up (exit 124, no abort) and reached `QG starting`. See [[startup-glib-gio-settings-crash]] |

## Playback

| Issue | Component | Status | Description |
|-------|-----------|--------|-------------|
| Browsing to another folder stopped playback (AUD-007) | `FolderModel`, `PlaybackController` | ✅ Fixed 2026-10-26 (FIX-PC-01 / AUD-007 half; static only) | `onModelStructureChanged()` treated the playing identity's absence from the *displayed* model as proof the file was deleted, so every navigation (sidebar, breadcrumb, folder double-click, Back/Forward) called `stop()` and closed the Stage/Cinema. `FolderModel` now publishes `folderPath` (`Q_PROPERTY` + `folderPathChanged`, from `FolderSnapshot::path`, emitted inside the model reset), and the controller only stops when the displayed model still represents the session's containing directory (`QFileInfo(session->path()).absolutePath()`) **and** the identity is missing. A live session on another folder also leaves the ordered context/Reel untouched, so the Reel keeps the playing folder's list (§9.1/§10.2/§19.4) — a deliberate deviation from the audit patch, which rebuilt it from the browsed folder. **Deferred:** the whole runtime check list (navigate away/back, Stage stays open, Reel re-anchors, genuine deletion still stops) plus `qg_reel_wiring`/`qg_playback_lifecycle` re-runs. See [[2026-10-26-fix-aud-007-playback-navigation]], `DEFERRED_TESTS.md` FIX-NAV-01 |

| Cinema never entered fullscreen (AUD-010) | `qml/QG/Main.qml` | ✅ Fixed 2026-10-27 (FIX-STAGE-01 / AUD-010 half; static only) | Cinema collapsed the chrome and grew the Stage but never touched the window state, so the "intensity" ran inside a decorated window and §4.2.1/§4.5 were unmet. `onCinemaActiveChanged` now remembers `window.visibility` (unless already `FullScreen`), sets `Window.FullScreen`, focuses the Stage and wakes both transport overlays (§4.4 — the cinema top bar's opacity is bound to `transport.opacity`), and on exit restores the remembered state (never a stale fullscreen) and focuses the content view. §4.2.6 holds by construction: no playback call was added. **Deferred:** the entire X11 verification (decorations, geometry restore, pointer/overlay auto-hide, no rebuffer) and the offscreen property assertion — `Window.FullScreen` is a no-op under `QT_QPA_PLATFORM=offscreen`. AUD-011 (Esc semantics) is deliberately untouched. See [[2026-10-27-fix-aud-010-cinema-fullscreen]], `DEFERRED_TESTS.md` FIX-STAGE-01 |

## Thumbnails

| Issue | Status | Description |
|-------|--------|-------------|
| VIS-010 — ready rows painted no imagery | ✅ Resolved 2026-10-22 | `ThumbnailRole` carried a `QImage`; `Image.source` needs a `QUrl`, so the source was empty while the row claimed `Ready` and hid its placeholder. Now published as `file://` URLs. See [[video-thumbnails-not-rendered]] |
| Warm start re-extracts every thumbnail | ✅ Fixed 2026-10-26 (FIX-TH-01, static only) | The QG PNG + DB cache was **write-only**: `runExtraction()` only consulted the freedesktop cache, while `ThumbnailRepository::lookup()` and `decideThumbnail()` had no production caller, so every navigation re-decoded every visible row and a corrupt file was retried forever. Now `consultQgCache()` (GUI thread) reads the persisted row, validates it against the current `(mtime_ns, size)` through the §18.5 policy, and either serves the cached PNG (read on the media pool, not re-encoded), reports a terminal failure without decoding, or falls through to the freedesktop cache/extractor. **Deferred:** the 200-video warm-start check (no FFmpeg open on the second visit), the bounded-retry/terminal proof, and the touched-suite re-runs. Two follow-ups stay open: `publishThumbnail()` still encodes on the GUI thread for fresh extractions (FIX-TH-02), and the in-memory decoded LRU is populated but never read. See [[2026-10-26-fix-aud-008-thumbnail-cache]] |
| Probe cache not invalidated on source change | 🟡 Open | THUMB-007 is only half covered: thumbnail invalidation is verified, probe invalidation is not. |

## Concurrency

| Issue | Component | Status | Description |
|-------|-----------|--------|-------------|
| Intermittent SEGFAULT in worker-pool delivery | `src/workers/tasks.cpp` (`submitTask`) | ✅ Resolved 2026-10-25 | Queued delivery invoked a **raw `QObject *receiver`** from a pool thread, so the worker read the receiver's thread affinity (and posted to freed memory if it was gone): ~4 % of `qg_keyboard_test` teardown runs crashed in `QObject::thread()`, and the deterministic repro crashes in `notify_helper()` on the GUI thread instead. Delivery now goes through a process-lifetime GUI-thread hub with a `QPointer` guard built at submission; the guard is re-checked on the GUI thread. Pre-fix ASan control SEGV 3/3; post-fix 0 crashes in 200 plain + 125 ASan runs. See [[worker-pool-receiver-use-after-free]], [[2026-10-25-fix-worker-delivery-uaf]] |
| **AUD-017 — persistence write errors swallowed** | `database` / `repositories` | ✅ Resolved in FIX-DB-01 (full build + focused tests) | `Statement::exec()` propagates mutation errors; `step()` remains SELECT iterator. Persistence 9/9 includes SQLite `query_only` write failure; probe 6/6; state controller 7/7. Full CTest, visible toast and probe failure-injection integration are unverified. |
| **AUD-018 — fallback trash metadata/restore integrity** | `src/filesystem/trash.cpp` | ✅ Resolved in FIX-FO-01 trash portion (full build + focused tests) | `.trashinfo` is written before move, failure leaves source in place; restore confines payload to canonical trash files root and removes consumed metadata. Deterministic metadata-path obstruction/outside-root tests passed. Full CTest/visible toast unverified; EXDEV is AUD-019 and remains open. |


| AUD-001 — dangling seek arguments / sync seek / no rejection feedback | `PlaybackManager::seek()`, `seekAbsolute()` | ✅ Resolved 2026-10-25 | `QByteArray::number(x).constData()` was stored in a `const char *args[]` initialised one statement before `mpv_command()` read it → ASan-confirmed heap-use-after-free (standalone probe). Seeks now go through a named argument buffer and `mpv_command_async` (libmpv documents sync calls as blocking for an unbounded time), a manager-owned seekability policy with `seekRejected()` wired to the transport bar's amber affordance, and DESIGN §3.1.4 consolidation (one command per 120 ms window, newest target wins, relative seeks accumulate on the intended position). 15 new policy tests; `qg_transport_state` 10/10 real-media runs; ASan+UBSan clean. See [[2026-10-25-fix-aud-001-seek-memory-safety]] |
| `qg_watcher` flaky (`modelDiffPreservesUnaffectedRows`) | `tests/unit/test_watcher.cpp` | 🟡 Open (pre-existing, host-dependent) | Fails 25/25 standalone with a pre-change binary too; no seek/submitTask involvement |
| AUD-003 — file operations ran on the GUI thread | `FileOperationsService` (`rename`/`move`/`copy`/`deletePermanently`) | ✅ Resolved 2026-10-25 (async half) | Each `Q_INVOKABLE` performed its whole walk/copy/delete inline — measured **818 ms** of GUI-thread block for a 3 MB / 400-file copy, 6.0 s for 24 MB / 3,000 files — and `operationFinished` re-entered QML before the call returned. Now submitted to `WorkerPools::io()` via `submitTask` with `this` as the lifetime-guarded receiver. Pre-fix control red on 4/4 discriminating cases; `qg_file_ops_async` 8/8, 25/25 repeats, ASan+UBSan clean. **Still open:** progress/cancellation + §8.2 partial summary (FIX-FO-02), `EXDEV` fallback (FIX-FO-01), `reveal`/`openWith` blocking launches. See [[2026-10-25-fix-aud-003-file-ops-async]] |
| `qg_video_surface_offset` flaky (`decodedFramesRenderAtItemRect`) | `tests/unit/test_video_surface_offset.cpp` | 🟡 Open (pre-existing, software GL) | Passes with 6592 bright pixels or fails with 0 after a 12.5 s wait, on the pre-change binary as well; was invisible while the GL suites QSKIPped without Xvfb |

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
