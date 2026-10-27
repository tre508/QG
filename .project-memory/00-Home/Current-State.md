---
type: state
project: QG
created: 2026-10-20
updated: 2026-10-27
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

**Last executed run (2026-10-25, FIX-SEL-01 / AUD-004)**: `DISPLAY=:77 ctest --test-dir build-work -E '^qg_qml$'` → **48/49** (only the known host-flaky `qg_watcher::modelDiffPreservesUnaffectedRows`); `DISPLAY=:77 ./build-work/tests/qg_qml_test -input tests/qml` → **200 passed / 8 failed**, the identical set to the pre-fix control run. Selection rendering was additionally proven from pixels (16/16, cyan ring + signal-blue border in Gallery/Standard/Compact/Manifest, plus an in-frame control that renders nothing with the old binding shape). See [[2026-10-25-fix-aud-004-selection-focus]].

**AUD-009 reflow (FIX-STAGE-01, same day)**: `ContentView.reservedTop`/`topInset` are applied to grid, compact and Manifest, and `Main.qml` binds the inset to `stage.height + Theme.space3`. Harness `aud009_reflow` **6/6** with the real Stage (372 px) at a 1200×620 region over 40 entries: `grid.y == 408`, row 0 entirely below the Stage, the last card reachable with no overlap, scroll+selection survive closing, Cinema reserves 0. The harness also caught and fixed a visible-range defect (the reporter double-subtracted the top margin, which at a ~400 px inset suppressed the first screen's report entirely). Full QML suite stayed **200/8**, CTest **48/49**. See [[2026-10-25-fix-aud-009-stage-reflow]].
**AUD-005 search/Manifest (FIX-SRCH-01, 2026-10-26 — static only)**: `SearchController` had **no `Q_PROPERTY` at all**, so `Main.qml`'s `manifestActive` (`searchController.active === true`) was `undefined` forever, and `setFolderName`/`setSortLabel` were plain members, which threw inside `onActiveChanged` before `enterManifest()`. Both causes are fixed: `active`/`summary`/`resultCount` are notifying properties (member set before the notification, de-duplicated; `clear()` re-notifies `resultsChanged`), the setters are `Q_INVOKABLE`, and the shell reads the summary and applied query from the properties. Checks run: `g++ -fsyntax-only` exit 0 with a red control; `qmllint` identical diagnostic sets vs the pre-fix worktree (128/74/139). **Nothing was built, run or launched** — the §7.2 presentation path, focus ownership between the field and the manifest list, and §7.3 row focus are all recorded as deferred checks. See [[2026-10-26-fix-aud-005-search-manifest]].

**AUD-006 persisted favorite/watched/progress (FIX-AUD-006, 2026-10-26 — static only)**: `StateController::loadIntoModel()` ran exactly once, in `CoreServices::wire()` (`src/core/composition.cpp:98`), before the first `navigate()` populated the model, so the decoration loop never executed; every later `applySnapshot` → `modelReset` replaced the rows without re-applying state, and hearts, watched checks and progress bars stayed empty for the whole session. Fix: `setModel()` now subscribes to the canonical model's `modelReset` and `rowsInserted` signals, clears `m_applied` on model swap/reset, and `scheduleLoad()` applies persisted state to newly visible rows exactly once per identity per generation on a coalesced zero-delay `QTimer`. `loadIntoModel()` is preserved as the immediate entry point for existing tests; `tests/` untouched. Root cause confirmed at `composition.cpp:98`. Nothing was built, run or launched — first-load/navigation/refresh restore, restart persistence, and first-paint on a 5,000-entry folder are recorded as deferred checks in `DEFERRED_TESTS.md` FIX-AUD-006. See [[2026-10-26-fix-aud-006-persisted-state]].
**AUD-007 playback vs navigation (FIX-PC-01 / AUD-007 half, 2026-10-26 — static only)**: `PlaybackController::onModelStructureChanged()` read "the playing identity is not in the displayed model" as "the file was deleted", so **every** navigation called `stop()` (playback ended, Stage closed, Cinema exited — §13.1 violated). `FolderModel` now publishes `folderPath`/`folderPathChanged` from `FolderSnapshot::path` (emitted inside the reset) and `clear()` drops it; the controller stops only when the displayed folder is the session's containing directory (`QFileInfo(session->path()).absolutePath()`) and the identity is genuinely absent. A live session on another folder additionally leaves the ordered context/Reel alone so the Reel keeps the playing folder's list (deliberate deviation from the audit patch). Checks run: `g++ -std=c++20 -fsyntax-only` exit 0 on both translation units, with red controls (renames → exit 1). **Nothing was built, run or launched**; the navigation/Stage/Reel/deletion checks and the `qg_reel_wiring` + `qg_playback_lifecycle` re-runs are in `DEFERRED_TESTS.md`. See [[2026-10-26-fix-aud-007-playback-navigation]].
**AUD-008 thumbnail cache serve-path (FIX-TH-01, 2026-10-26 — static only)**: the QG PNG + DB cache was **write-only** — `runExtraction()` consulted only the freedesktop cache, and `ThumbnailRepository::lookup()`/`decideThumbnail()` had no production caller — so every navigation re-decoded every visible row and a corrupt file was re-extracted forever. Now `consultQgCache()` (GUI thread, because `Database` is a single thread-affine connection) reads the persisted row, validates it against the current `(mtime_ns, size)` via the §18.5 policy and returns Serve / Terminal / Extract; the media pool reads the resolved PNG (never re-encoded on a served hit), a terminal marker is reported without a decode, and a manual Retry still re-extracts. Checks run: `g++ -std=c++20 -fsyntax-only` exit 0 on both translation units with five red controls (including a control proving the audit's `failed.error = QStringLiteral(…)` line does not compile). **Nothing was built, run or launched**; the 200-video warm-start proof (no FFmpeg open on the second visit), the terminal-marker proof and the touched-suite re-runs are in `DEFERRED_TESTS.md`. See [[2026-10-26-fix-aud-008-thumbnail-cache]].
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
| Warm start re-extracts every thumbnail | ✅ Fixed 2026-10-26 (FIX-TH-01; **static only**) | The PNG + DB cache was write-only: `runExtraction()` read only the freedesktop cache and `ThumbnailRepository::lookup()`/`decideThumbnail()` had no production caller. `consultQgCache()` (GUI thread — `Database` is thread-affine) now validates the persisted row against the current `(mtime_ns, size)`, serves the cached PNG (loaded on the media pool, not re-encoded), reports a terminal failure without decoding, or extracts. **Deferred:** the 200-video warm-start check and the bounded-retry proof. **Still open:** `publishThumbnail()` encodes on the GUI thread for fresh extractions (FIX-TH-02) and the decoded LRU is never read. See [[2026-10-26-fix-aud-008-thumbnail-cache]] |
| VIS-011/012 — card placeholders & badge zones | ✅ Implemented (static review) 2026-10-22 | Centre zone is now exclusive (`playGlyphVisible`), cues derive from `thumbnailState`, TR hover chip is 22 px in one chip background, watched badge holds a fixed slot, meta line anchors to the filename. Runtime screenshots deferred. See [[card-zone-collisions]] |
| VIS-010 — thumbnails never rendered | ✅ Resolved 2026-10-22 | Model exposed a `QImage` to `Image.source` (a `QUrl`) → empty source on `Ready` rows. Thumbnails now ship as `file://` URLs published by the controller; verified end-to-end under Xvfb with a colour-coded clip (98.6 % of the painted block matched the expected frame colour). See [[video-thumbnails-not-rendered]] |
| Worker-pool delivery use-after-free (intermittent SEGFAULT) | ✅ Resolved 2026-10-25 | `submitTask` queued results on a raw `QObject *receiver` from a pool thread, so the worker read the receiver's thread affinity; ~4 % of `qg_keyboard_test` teardown runs crashed. Delivery now uses a process-lifetime GUI-thread hub plus a `QPointer` guard built at submission and re-checked on the GUI thread. Pre-fix ASan control SEGV 3/3; post-fix 0 crashes in 200 plain + 125 ASan runs. See [[worker-pool-receiver-use-after-free]], [[2026-10-25-fix-worker-delivery-uaf]] |
| AUD-001 seek memory-safety/reliability | ✅ Resolved 2026-10-25 | Dangling `QByteArray` argument in `seek()`/`seekAbsolute()` (ASan-confirmed), synchronous dispatch, and missing §3.1.4 consolidation. Seeks now use a named argument buffer with `mpv_command_async`, a manager-owned non-seekable policy with `seekRejected()` wired to the transport bar, and one-command-per-120 ms-window consolidation (newest target wins; relative seeks accumulate on the intended position). 15 new policy tests, 10/10 real-media runs, ASan+UBSan clean. See [[2026-10-25-fix-aud-001-seek-memory-safety]] |
| AUD-005 Manifest intensity unreachable | ✅ Implemented 2026-10-26 (FIX-SRCH-01; **static only**) | `SearchController` had no `Q_PROPERTY`, so `manifestActive` was always false, and its two context setters were not invokable, so the activation handler threw before `enterManifest()`. Now `active`/`summary`/`resultCount` are notifying properties and the setters are `Q_INVOKABLE`. **Deferred:** the §7.2 presentation path, focus ownership (field vs `manifestList`) and §7.3 row focus (§7.3 is a separate finding). See [[2026-10-26-fix-aud-005-search-manifest]] |
| AUD-006 persisted favorite/watched/progress never applied after navigation | ✅ Implemented 2026-10-26 (FIX-AUD-006; **static only**) | `StateController::loadIntoModel()` ran once in `CoreServices::wire()` (`src/core/composition.cpp:98`) before the first `navigate()` populated the model, so the decoration loop never executed and later model resets never re-applied state. `setModel()` now subscribes to `modelReset`/`rowsInserted`, clears `m_applied` on reset, and `scheduleLoad()` decorates new rows exactly once per identity per generation on a coalesced zero-delay timer; `loadIntoModel()` preserved for tests. **Deferred:** first-load/navigation/refresh restore, restart persistence, first-paint on a 5,000-entry folder. See [[2026-10-26-fix-aud-006-persisted-state]] |
| AUD-010 Cinema never entered fullscreen | ✅ Implemented 2026-10-27 (FIX-STAGE-01 / AUD-010 half; **static only**) | Cinema hid the chrome and grew the Stage but left the window decorated: §4.2.1 ("enter fullscreen using the existing window") and §4.5 were unmet, and the audit's own grep found no `showFullScreen`/`Window.FullScreen` anywhere. `Main.qml` now carries `windowedVisibility` and `onCinemaActiveChanged` fullscreens the existing window on entry (remembering the prior state unless already fullscreen), focuses the Stage, wakes both transport overlays (§4.4), and restores the remembered state on exit — never a stale fullscreen — then focuses the content view. No playback call, no second window, no Stage/Cinema layout change. **Deferred:** everything needing a real WM (decorations, exact geometry restore, pointer/overlay auto-hide, no rebuffer) plus the offscreen property assertion (`Window.FullScreen` is a no-op under `QT_QPA_PLATFORM=offscreen`). See [[2026-10-27-fix-aud-010-cinema-fullscreen]] |
| AUD-003 file operations on the GUI thread | ✅ Resolved 2026-10-25 (async half) | `rename`/`move`/`copy`/`deletePermanently` ran their whole walk/copy/delete inside the `Q_INVOKABLE` call — measured **818 ms** of GUI-thread block for a 3 MB / 400-file copy (6.0 s for 24 MB / 3,000 files) and `operationFinished` re-entered QML before the call returned. They now submit to `WorkerPools::io()` via `submitTask` and deliver the result on the GUI thread after the event loop turns. Pre-fix control red on 4/4 discriminating cases; new `qg_file_ops_async` 8/8 + 25/25 repeats + ASan/UBSan clean. Progress/cancellation and the §8.2 partial summary stay open (FIX-FO-02). See [[2026-10-25-fix-aud-003-file-ops-async]] |
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
