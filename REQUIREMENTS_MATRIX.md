# QG Requirements Matrix

Traceability: `DESIGN.md` requirement → planned implementation prompt → planned validation. Statuses: `PLANNED`, `PARTIAL`, `IMPLEMENTED`, `VERIFIED`, `BLOCKED`.

Legend — Planned Prompt(s) reference `dev-guide.md`; Design Source references `DESIGN.md`.
Reconciled 2026-10-08 from executed validation of P01–P34 (CTest 26/26 with `TMPDIR=/tmp`; `qg_qml` 65/65 subtests). `VERIFIED` = covered by passing automated tests; `IMPLEMENTED` = code exists but its full gate (usually a checkpoint or a later wiring prompt) has not yet run; `PARTIAL` = some listed prompts complete, others pending.

P22–P30 reconciliation (2026-10-08): StatusStrip (P22, CP-C), and the thumbnail/metadata/persistence layer — freedesktop reader + cache key (P23), in-process extraction (P24), visible-first scheduler + bounded LRU (P25), failure/retry/invalidation (P26), validated model wiring (P27), in-process probe service (P28), identity↔path mapping + reconciliation (P29), and StateController favorite/watched/progress (P30). CP-C and CP-D COMPLETE. Thumbnail/probe controllers are implemented and unit-validated but not yet instantiated by `App` or driven from the `ContentView` viewport (planned P32/performance prompt), so their end-to-end rows remain `IMPLEMENTED`.

P31–P34 reconciliation (2026-10-08): per-folder sort + density persistence (P31), probe→model wiring with `--:--` unknown handling and a coalesced resort hook (P32), `SortService` deterministic total order wired through `FolderModel::reorder` (P33), and the PathBar Order menu + `Ctrl+O` wiring with per-folder persistence (P34). `SORT-001/002/003/005`, `META-002/003`, and `PERSIST-005` advance to `VERIFIED`; `SORT-004` stays `PARTIAL` (P49/P55 Reel/PlaybackContext pending).
FIX-UI-004 reconciliation (2026-10-21): Standard-density grid geometry
recomputed and centred (VIS-008/009). Columns now derive from the §3.5 band
target (240/220/200/190) instead of `min_card`, and the occupied grid is centred
with a single `leftMargin` (`rightMargin` 0). Measured at 1593 px workspace:
6 columns × 244 px with 24.5/24.5 gutters (was 8 nominal → 7 actual × 179 px with
a ~219 px dead right band). `tst_grid_geometry.qml` 13/13 PASS (11 measured test functions);
`tst_contentview`/`tst_density`/`tst_scaling`/`tst_main`/`tst_virtualization`
pass apart from the documented pre-existing ContentView mock failure. Screenshot
comparison UNVERIFIED (no graphical session).

FIX-UI-003 reconciliation (2026-10-21): normalized sidebar icons, device
classification and mount states (VIS-005/006/007). Places rows now render the
semantic monoline icon the model publishes (11 new SVGs authored + registered in
qrc and verified to rasterize) instead of one hardcoded glyph; devices are
classified by mount identity (`System`/`Internal`/`Removable`/`Optical`/
`Network`/`Image`/`Virtual`) with removable and eject/unmount capability taken
from the kernel (`/sys`, `/dev/disk/by-label`) and GVfs/UDisks2
(`gio mount -l -i`); loopback snap images and desktop plumbing are classified but
not surfaced as sidebar devices (59 mounts → 6 device rows on this host); group
headings are model rows so the list's content height is exact and the last row is
reachable; disconnected volumes are retained with an amber badge and reconnect
restores them; pinned locations are now projected from the SQLite bookmark
repository. `qg_mounts` 14/14, `qg_places_model` 14/14, sidebar QML 16/16, icon
QML 6/6, full QML 187 passed (4 pre-existing failures unchanged). Real eject and
real-desktop screenshot validation remain UNVERIFIED (no disposable device, no
desktop session).

FIX-UI-002 reconciliation (2026-10-21): restored the Path Bar navigation group (back/forward/parent/refresh), fixed the doubled root slash in the breadcrumb (separator now "›", root appears once), and implemented the Density + Overflow actions that were empty placeholder slots. Registered all 37 monoline SVGs in the qrc (was 16) so icons no longer fall back to dots. `UI-002`, `UI-008`, `NAV-002/003`, `SEARCH-001` remain `VERIFIED` (implementation restored, no requirement-state change). Visual/screenshot validation BLOCKED (headless; full app does not map under Xvfb).

## Product Invariants

| ID | Requirement | Design Source | Planned Prompt(s) | Planned Test/Validation | Implementation | Tests | Status |
|---|---|---|---|---|---|---|---|
| INV-001 | Filesystem is the source of truth; QG never imports/relocates media | §1.1, §17 | P09, P10, P59 | enumeration + operations tests | `filesystem/enumerator`, `models` | enum integration | PARTIAL |
| INV-002 | One surface, four intensities (Browse/Stage/Cinema/Manifest) | §1.2, §7.2 | P16, P45, P52, P54 | presentation-swap tests | ContentView: all four present; the Manifest intensity is reachable since FIX-SRCH-01 (2026-10-26) — `SearchController` publishes `active`/`summary`/`resultCount` as properties and the shell binds `manifestActive` to them | QML integration | IMPLEMENTED (static; presentation-swap run deferred — `DEFERRED_TESTS.md` FIX-SRCH-01) |
| INV-003 | One mpv handle / one active playback session | §19.2 | P46, P48, P52 | handle-count test | `playback/mpv_backend` | playback-manager | PLANNED |
| INV-004 | GUI thread never waits | §15.3, §22.1 | P04, P09, P24, P28, P69 | off-thread assertions | workers/services (adaptive/thumbnail/probe pending) | perf + unit | PARTIAL (AUD-003 **FIXED 2026-10-25**: all four mutating file-operation entry points now submit to the I/O pool through `submitTask` and deliver on the GUI thread — `qg_file_ops_async` proves the call returns before the work is delivered, that delivery lands on the GUI thread exactly once, and that a 1 ms watchdog keeps ticking during a real 400-file copy; pre-fix control blocked the GUI thread for 818 ms on the same fixture. **Still on the GUI thread:** probe (AUD-021), PNG encode (AUD-008), `reveal`/`openWith` launches, inline `TrashService`) |
| INV-005 | One authoritative ordered source (FolderModel) for all consumers | §20.1 | P10, P33, P49 | order-equality tests | `FolderModel` (grid, PlaybackContext, Reel) | sort/context | VERIFIED (REMEDIATION-007: `qg_playback_context` + `qg_reel_wiring` assert the context and Reel are the model's row order, videos only) |

## Architecture

| ID | Requirement | Design Source | Planned Prompt(s) | Planned Test/Validation | Implementation | Tests | Status |
|---|---|---|---|---|---|---|---|
| ARCH-001 | Native C++20 application stack | §15.5 | P01, P02 | build + run | build/app | build smoke | VERIFIED |
| ARCH-002 | Qt 6 Quick/QML presentation only | §15.1 | P03, P17 | QML load tests | `qml/QG` | QML test | VERIFIED |
| ARCH-003 | Layered boundaries (qml/controllers/models/services/workers/persistence/playback) | §15.1 | P02, P13, P28, P46 | drift audits CP-A..K | tree; playback+search layers compiled & composed (REM-001/003); `services/` dir still absent (D-17) | checkpoint audit | PARTIAL |
| ARCH-004 | Qt Quick forced to OpenGL before first window | §15.1, §19.3 | P02, P47 | startup + render | `main.cpp`, `app.cpp` | manual/render | VERIFIED |
| ARCH-005 | Target-based CMake + Ninja | §15.5, §16.1 | P01 | configure/build | `CMakeLists.txt`, `cmake/Dependencies.cmake`, `CMakePresets.json` | build + ctest | VERIFIED |
| ARCH-006 | Two bounded worker pools + TaskToken | §15.3, §22.2 | P04 | cancellation/concurrency tests | `src/workers/*` | unit | VERIFIED |
| ARCH-007 | Typed domain errors + cooperative cancellation | §15.4 | P05, P04 | error/cancel tests | `core/errors.hpp`, `workers` | unit | VERIFIED |
| ARCH-008 | Project file tree by responsibility | §16 | P01 | layout inspection | tree | review | IMPLEMENTED |
| ARCH-009 | QML module registration (`qt_add_qml_module`) | §15.5 | P03 | QML load | `qml/CMakeLists.txt`, `qml/QG/qmldir` (generated) | QML test | VERIFIED |

## Filesystem & Navigation

| ID | Requirement | Design Source | Planned Prompt(s) | Planned Test/Validation | Implementation | Tests | Status |
|---|---|---|---|---|---|---|---|
| FS-001 | Each directory lists every child directory + supported video, nothing else | §5.1, §17.2, §25.1 | P07, P09 | temp-dir enum tests | `filesystem/enumerator` | integration | VERIFIED |
| FS-002 | Child directories always shown, even empty | §1.1, §5.2, §25.1 | P09, P19 | folder-only dir test | enumerator/model | integration | VERIFIED |
| FS-003 | Unsupported regular files filtered at enumeration | §5.1, §17.3 | P07, P09 | filter test | `media/formats.hpp` | unit | VERIFIED |
| FS-004 | Incremental batches (~200) for first-screen responsiveness | §17.2, §22.4 | P09 | batch-signal test | enumerator | integration | VERIFIED |
| FS-005 | Per-entry errors do not abort the scan | §17.2 | P09 | broken symlink/permission test | enumerator | integration | VERIFIED |
| FS-006 | Hidden files shown only when enabled (default off) | §17.2 | P09 | toggle test | enumerator | unit | VERIFIED |
| FS-007 | Mount discovery via mountinfo + GIO/UDisks2 enrichment | §17.4 | P11 | fixture mountinfo test | `filesystem/mounts` | unit | VERIFIED (FIX-UI-003: `MountService` now actually installs the enrichment — kernel `/sys/class/block/<disk>/removable` + `/dev/disk/by-label` labels, then GVfs/UDisks2 `gio mount -l -i` for labels/removable/eject/unmount; `qg_mounts` covers parsing + capability application with fixtures and `realHostClassificationReport` classifies the live mount table) |
| FS-008 | Debounced (300ms) identity-based refresh diff | §17.5 | P12 | external-change test | `filesystem/watcher` | integration | VERIFIED |
| FS-009 | `F5` forces full re-enumerate | §17.5 | P12, P36 | refresh test | watcher/controller (key binding P36) | integration | PARTIAL |
| FS-010 | File operations use real filesystem semantics | §17.6 | P59 | temp-dir ops tests | `filesystem/operations` | integration | PLANNED |
| FS-011 | Trash via freedesktop/gio, undoable | §17.7 | P60, P64 | trash+undo tests | `filesystem/trash` | integration | PARTIAL (REMEDIATION-010: `qg_action_feedback::trashThenUndoRoundTrips` proves the spec-compliant Trash writes `.trashinfo` and Undo restores the file in place; the cross-device `gio trash` branch reports success with **no** undo path — and the toast then offers no Undo button) |
|| FS-012 | No silent permanent deletion | §17.7 | P60, P65 | confirm-gate test | trash/dialogs | integration | VERIFIED (REM-14: `ConfirmDeleteDialog` is the ONLY route to `FileOperationsService::deletePermanently` — reachable from `Shift+Delete` and the context menu only after explicit confirm; `qg_action_feedback::permanentDelete*` prove the file is really removed, a missing path is an explicit failure + toast, and a mixed set is a partial summary with survivors intact) |
| FS-013 | Stable identity `(dev,ino)` + fingerprint fallback | §17.8 | P05, P29 | identity tests | `core/identity` | unit | VERIFIED |
| FS-014 | Broken symlinks badge without breaking enumeration | §25.1 | P09, P18 | symlink test | enumerator/delegate | integration | VERIFIED |
| FS-015 | Symlinked directories navigable | §25.1 | P09 | symlink test | enumerator (follows symlinks) | integration | IMPLEMENTED |
| NAV-001 | Navigation stack back/forward/parent, truncates on navigate | §17.1, §25.1 | P13, P14 | nav-stack test | BrowserController | integration | VERIFIED |
| NAV-002 | `Ctrl+L` path entry navigates absolute paths | §13.2, §23.2 | P14 | path-entry test | PathBar | QML test | VERIFIED |
| NAV-003 | Invalid paths error inline without navigating | §25.1 | P12, P14 | invalid-path test | PathBar/controller | QML test | VERIFIED |
| NAV-004 | Recent locations persisted (capped, deduped) | §17.1 | P13, P31 | persistence test | repositories | integration | VERIFIED |
| NAV-005 | Enter child folder updates view + focus restore | §23.3, §13.3 | P13, P16, P36 | nav test | controller/model (focus restore P36) | integration | PARTIAL |

## Main Window / UI

| ID | Requirement | Design Source | Planned Prompt(s) | Planned Test/Validation | Implementation | Tests | Status |
|---|---|---|---|---|---|---|---|
| UI-001 | Six-region main window composition | §3.1 | P03 | QML load | `Main.qml` | QML test | VERIFIED |
| UI-002 | Path Bar doubles as toolbar/location (no conventional toolbar) | §3.2 | P14 | QML test | `PathBar.qml` | QML test | VERIFIED |
| UI-003 | Sidebar rail/panel + responsive thresholds | §3.3 | P15 | QML test | `PlacesSidebar.qml` | QML test | VERIFIED (FIX-UI-003: icons+labels, capability-gated eject/unmount, amber disconnected badge, group headings as model rows, exact scroll range, heading-skipping keyboard nav; `tst_places_sidebar` 16/16) |
| UI-004 | Context strip (folder, counts, order, density) | §3.6 | P21 | QML test | `ContextStrip.qml` | QML test | IMPLEMENTED (FIX-VIS-014: density options now carry tooltip + accessible name, Accessible.checkable/checked, an active chip, and fit the 28 px strip. Re-verification deferred) |
| UI-005 | Status strip (selection, free space, activity, transient) | §3.6 | P22 | QML test | `StatusStrip.qml` | QML test | IMPLEMENTED (the selection count and aggregate size now track the selection: FIX-SEL-01 2026-10-25 made `SelectionState` observable and `FolderModel::indexOfIdentity` Q_INVOKABLE) |
| UI-006 | Responsive columns 700–1920px, cards ≤320px, aspect preserved | §3.5, §25.2 | P17 | column-recompute test | `ContentView.qml` | QML test | VERIFIED (FIX-UI-004: column count derives from the §3.5 band target — the `min_card`-driven count produced 8×179 px at 1593 px; now 6×244 px. `tst_grid_geometry` measures, at 7 window widths, that computed columns == laid-out columns, gutters are equal within 1.5 px, no card crosses the viewport edge, thumbnails stay 16:9 and the metadata band stays 44 px) |
| UI-007 | View virtualization (no QML object per row) | §22.4 | P17 | large-model test | `ContentView.qml` (GridView) | perf/QML | VERIFIED (FIX-UI-004: `tst_grid_geometry::test_largeModelRemainsVirtualized` drives a 1000-row model at 1593×900 and asserts the GridView instantiates < 200 delegates while exposing all 1000 rows) |
| UI-008 | Density modes Gallery/Standard/Compact, per-folder | §5.4 | P20 | density test | ContentView delegates | QML test | IMPLEMENTED (FIX-VIS-013: the Path Bar button now shows the mode-specific shipped icon and a 'Density: <mode>' tooltip instead of the static generic grid; cycling/persistence re-verification deferred) |
| UI-009 | Selection/hover/focus states distinct | §5.5 | P17, P18, P35 | state test | delegates (SelectionState P35) | QML test | VERIFIED (FIX-SEL-01 2026-10-25: rendered-pixel probe shows the cyan focus ring (`Theme.focus`) and the signal-blue selection border on different cards in the same frame; `render_probe` 9/9) |
| UI-010 | Empty/error/loading state presentations | §12 | P67 | state-render test | `StateOverlay.qml` | QML test | VERIFIED (REMEDIATION-010: `Main.qml` derives `contentState` with the §12 precedence — disconnected > accessDenied > error > loading (only while nothing is shown) > empty — drives `StateOverlay` and a thin indeterminate bar, and suppresses every overlay over rendered content; `tst_overlays_dialogs` asserts each state, the precedence, and the overlay actions) |
| UI-011 | Semantic theme tokens (Theme.qml), no raw hex outside | §2, App.A | P03 | token audit | `Theme.qml` | review | IMPLEMENTED |
|| UI-012 | Motion + reduced-motion respect | §2.8 | P53, P38 | reduced-motion test | animations | QML test | VERIFIED (`tst_accessibility`: `reducedMotion` toggle drives `motionEnabled`; consumers Stage/Main gate `Behavior.enabled`/pulse on it; `motionEnabled===false` → instant swap. Qt 6.10 exposes NO reduced-animation hint, so the app toggle is authoritative — platform clause recorded as unavailable) |
|| UI-013 | Accessibility (roles, contrast, non-color-only) | §2.9, §13.4 | P38 | a11y tests | QML controls | QML test | VERIFIED (`tst_accessibility` asserts card Accessible.name/role; `highContrast` now binds `Qt.styleHints.accessibility.contrastPreference` and visibly changes surface/neon token values — the toggle is wired, not dead) |

## Cards

| ID | Requirement | Design Source | Planned Prompt(s) | Planned Test/Validation | Implementation | Tests | Status |
|---|---|---|---|---|---|---|---|
| CARD-001 | Video card anatomy + fixed badge zones | §6.1 | P18 | QML test | `VideoCard.qml` | QML test | IMPLEMENTED (FIX-VIS-011/012 revised the zone contract: fixed TL favourite/watched slots, 22 px TR hover chip, exclusive centre cue. The earlier QML verification predates this change — re-verification is queued in `DEFERRED_TESTS.md`) |
| CARD-002 | Full video-card state matrix | §6.2 | P18 | state matrix test | `VideoCard.qml` | QML test | IMPLEMENTED (states are now driven by `placeholderKind` derived from `thumbnailState` 0–4; centre cue exclusivity and the failure scrim are new. State-matrix screenshots are deferred) |
| CARD-003 | Folder card mosaic + count chip, no duration | §5.2 | P19 | QML test | `FolderCard.qml` | QML test | VERIFIED |
| CARD-004 | Metadata hierarchy (thumbnail, name, duration, size/date, res) | §5.6 | P18 | QML test | delegates | QML test | VERIFIED (VIS-010: `tst_videocard` asserts the thumbnail paints real pixels for a model URL, hides with no source, and fills the well with PreserveAspectCrop; duration/size/date/res chips covered by the same suite) |
| CARD-005 | Compact row aligned columns | §5.4 | P20 | QML test | `CompactRow.qml` | QML test | VERIFIED |

## Search / Manifest

| ID | Requirement | Design Source | Planned Prompt(s) | Planned Test/Validation | Implementation | Tests | Status |
|---|---|---|---|---|---|---|---|
| SEARCH-001 | `Ctrl+F` morphs breadcrumb into field (no new bar) | §7.1, §25.4 | P42 | QML test | `PathBar.qml` | QML test | VERIFIED |
| SEARCH-002 | In-memory filter, <100ms on 10k entries | §20.3, §25.4 | P39, P41 | latency test | `search/search_service` | perf/unit | PARTIAL |
| SEARCH-003 | Case-insensitive substring + range highlight | §7.1, §20.3 | P39, P43 | match/range test | SearchService/ManifestRow | unit/QML | PARTIAL |
| SEARCH-004 | Manifest columnar layout + sortable header | §7.3 | P43 | QML test | `qml/QG/delegates/ManifestRow.qml` + the header/ListView at `ContentView.qml:722-921` (present at HEAD) | QML test | IMPLEMENTED (static; §7.3 **row focus** — ↑/↓ and Enter on a Manifest row — has no `Keys` contract on `manifestList`, recorded as a dependent finding) |
| SEARCH-005 | No-results state with Clear (never "No videos found") | §7.4 | P44 | QML test | `qml/QG/overlays/NoResults.qml`, wired at `ContentView.qml:897-902`; the Clear action now routes through `ContentView.searchClearRequested` → the field → `SearchController` (2026-10-26) | QML test | IMPLEMENTED (static) |
| SEARCH-006 | Scope chip `This folder`; Subfolders present but inert | §7.1, §25.4 | P42 | QML test | `PathBar.qml:396-417` — chip present, but it is **live**, not inert: clicking it calls `setScope(true)` and starts a recursive walk (SEARCH-008 is implemented, §7.1's "coming soon" copy is absent) | QML test | PARTIAL (presentation matches; the "inert" half does not — either disable the chip or amend §7.1) |
| SEARCH-007 | Clear restores exact scroll/selection | §7.2, §25.4 | P44, P45 | restore test | `ContentView.enterManifest`/`exitManifest` — scroll is restored from `savedGridViewY`, **selection is not** (`enterManifest` clears it; AUD-023/FIX-SRCH-02). Reachable at all only since FIX-SRCH-01 (2026-10-26) | integration | PARTIAL |
| SEARCH-008 | Recursive-search interface reserved (not implemented) | §20.4 | P39 | API presence test | `search/search_service` | unit | IMPLEMENTED |
| SEARCH-009 | `SearchProxy` provides filtering, match-range role, result cap | §14.2, §22.6 | P40 | proxy model test | `search_proxy` | unit | VERIFIED |

## Sorting

| ID | Requirement | Design Source | Planned Prompt(s) | Planned Test/Validation | Implementation | Tests | Status |
|---|---|---|---|---|---|---|---|
| SORT-001 | Deterministic stable total order | §5.7, §20.2, §25.5 | P33 | repeat-sort test | `sorting/sort_service` | unit | VERIFIED |
| SORT-002 | Folders-first toggle (default on) | §5.7, §20.2 | P33 | order test | SortService | unit | VERIFIED |
| SORT-003 | Per-folder sort + density persistence | §20.2, §21.3 | P31, P34 | reopen test | settings repo | integration | VERIFIED |
| SORT-004 | Browser = Reel = playlist = prev/next order | §20.1, §25.5 | P33, P49, P55 | order-equality test | model/context/reel | integration | VERIFIED (`qg_playback_context::followsTheActiveSortOrder` + `qg_reel_wiring`: context and Reel mirror the model's reordered rows; folders excluded) |
| SORT-005 | Unknown durations last (asc) + single coalesced resort | §20.2, §25.5 | P32, P33 | unknown-order test | SortService | unit | VERIFIED |

## Thumbnails

| ID | Requirement | Design Source | Planned Prompt(s) | Planned Test/Validation | Implementation | Tests | Status |
|---|---|---|---|---|---|---|---|
| THUMB-001 | Async thumbnails; GUI never blocks | §18, §25.3 | P24, P25 | 5k-dir responsiveness | `media/thumbnails` | perf/integration | VERIFIED (VIS-010: extraction runs on the bounded cpuMedia pool, one in-flight request, stale-generation rejection; `qg_thumb_delivery` uses real MP4/MKV fixtures and the real app produced thumbnails under Xvfb while staying responsive) |
| THUMB-002 | Freedesktop thumbnail cache read + `Thumb::MTime` validation | §18.2 | P23 | cache-reader test | thumbnails | unit | VERIFIED (VIS-010: `findFreedesktopThumbnail` validates Thumb::URI + Thumb::MTime; `freedesktopCacheHitSkipsExtraction` seeds a valid entry and asserts the injectable extractor is never called — the lookup now precedes extraction). FIX-TH-01 (2026-10-26): QG's **own** persisted cache is consulted before the freedesktop cache (and before the extractor), so the freedesktop branch is the second serve source, not the first |
| THUMB-003 | Cache key = identity + mtime_ns + size (+version) | §18.2 | P23 | key test | thumbnails | unit | VERIFIED (VIS-010: `sourceChangeProducesANewCacheIdentity` rewrites the source and asserts the published URL changes, so no stale imagery; `qg_thumbnails` covers the key encoding) |
| THUMB-004 | Visible-first priority scheduling + demotion | §18.3, §22.2 | P25 | scheduler test | ThumbnailService | unit | PLANNED |
| THUMB-005 | Stale-result validation (token, membership, source) | §18.4, §22.5 | P27 | stale-drop test | thumbnails/model | integration | VERIFIED (`qg_thumb_wiring` drops stale-generation and changed-source results; VIS-010 adds settled rows are never re-extracted and folder rows are never extracted) |
| THUMB-006 | Failure marker + bounded retry (≤2) | §18.5 | P26 | failure test | thumbnails/repo | integration | IMPLEMENTED (static; FIX-TH-01 2026-10-26) — the marker is now **consulted**: `consultQgCache()` runs `decideThumbnail()` on every request, so a failed file retries only within its budget (`1 + kMaxThumbnailFailRetries` decodes) and an exhausted marker returns Terminal with no decode at all; a source change or the card's manual Retry (now `ThumbnailRequest::manualRetry`) still re-extracts. `qg_thumb_failure` covers the policy in isolation and `qg_card_actions::thumbnailRetryReExtracts` the retry wiring; the runtime proof (no further decode across navigations) is deferred |
| THUMB-007 | Source change invalidates thumbnail + probe caches | §17.5, §25.3 | P26 | invalidation test | watcher/thumbnails | integration | PARTIAL (thumbnail half VERIFIED by `sourceChangeProducesANewCacheIdentity`; probe-cache invalidation still uncovered) |
| THUMB-008 | Bounded decoded-image LRU | §22.3 | P25 | LRU bound test | ThumbnailService | unit | PARTIAL (FIX-TH-01 2026-10-26: `storeDecoded()` is now also reached on cache-served rows — it already was on extractions — but the read side (`hasDecoded()`/`decoded()`) still has no production caller, and `ThumbnailScheduler::clear()` keeps the LRU across navigation, so a RAM serve path exists but is unwired) |

## Metadata

| ID | Requirement | Design Source | Planned Prompt(s) | Planned Test/Validation | Implementation | Tests | Status |
|---|---|---|---|---|---|---|---|
| META-001 | In-process FFmpeg probing (duration/resolution/rotation/tracks) | §18.6 | P28 | probe test | `media/probe` | integration | PLANNED |
| META-002 | Duration/resolution cached by identity+mtime/size | §18.6 | P28, P32 | cache test | probe/repo | integration | VERIFIED |
| META-003 | Unprobed items show `--:--`, placed last when sorting | §18.6 | P32 | unknown test | model/SortService | unit | VERIFIED |
| META-004 | Playback instance never used for probe/thumbnail | §18.3, §18.6 | P24, P28 | isolation test | probe/thumbnails | review/integration | PLANNED |

## Playback

| ID | Requirement | Design Source | Planned Prompt(s) | Planned Test/Validation | Implementation | Tests | Status |
|---|---|---|---|---|---|---|---|
| PLAY-001 | Exactly one mpv instance regardless of plays | §19.2, §25.6 | P46, P48 | handle-count test | `playback/mpv_backend` | playback-manager | VERIFIED |
| PLAY-002 | Inline Stage playback (no new window) | §8.1, §15.2 | P50, P52 | integration test | Stage/PlaybackController | integration | VERIFIED (2026-10-25: the Stage also occupies layout space now — `ContentView.reservedTop` reflows the grid beneath it, §3.4/§2.1.7; harness `aud009_reflow` 6/6, and the grid's first card maps to exactly `reservedTop + pad`). FIX-PC-01 (AUD-007 half, 2026-10-26): the Stage also stays open while another folder is browsed (§13.1) — implemented/static, runtime check deferred. The AUD-009 reflow inset itself has no in-tree gate (its harness lives outside the repo) |
| PLAY-003 | Transport controls effective + reflected in UI | §8.3, §25.6 | P51 | QML/keyboard test | TransportBar | QML test | VERIFIED (play/pause, ±10s, scrub, volume, mute, fullscreen, close by `tst_transport`+`tst_cinema`+`qg_transport_state`; Prev/Next now traverse the ordered context, by `qg_reel_wiring`) |
| PLAY-004 | Prev/Next follow folder sort + filter | §8.3, §19.4, §25.6 | P49, P56 | context-step test | PlaybackContext | unit/integration | VERIFIED (`qg_playback_context` order/filter/boundaries; `qg_reel_wiring` prev/next follow the displayed order under sort and the All/Favorites/Unwatched filter, stop at the ends). FIX-NAV-01 (2026-10-26): while another folder is displayed and a session is live, Prev/Next and `positionLabel` describe the **session's** folder list (§19.4) — a behaviour change, implemented/static, runtime check deferred |
| PLAY-005 | Position persisted ~5s + resumed when eligible | §19.6, §25.6 | P57 | resume test | PlaybackManager | integration | VERIFIED (`qg_resume_policy`: DB-backed lookup, eligibility window, Always/Ask/Never; `qg_resume_playback` (GL): a real 60s stream resumes near a stored 30s and starts at 0 under Never, pause writes the live position to SQLite, a relaunch reads it back; `qg_composition`: the composed manager resolves the stored position from the database) |
| PLAY-006 | Failed open → playback-failure state with recovery | §12, §25.6 | P52, P67 | failure test | Stage/StateOverlay | integration | PARTIAL (`qg_transport_state`: END_FILE error → `PlaybackSession::Error` + message + ⚠ state; the Stage's Next affordance now reaches an effective Next, but the full StateOverlay recovery flow is REMEDIATION-010) |
| PLAY-007 | Volume/mute/seek ±10s/scrub | §8.3 | P51 | control test | TransportBar/PlaybackManager | QML test + `qg_seek_policy` | VERIFIED (`tst_transport`: volume 0..1, mute, ±10s intents, optimistic+coalesced scrub; `qg_transport_state`: seek/pause take effect on a live stream; **AUD-001**: `qg_seek_policy` 15/15 — normalisation, duration clamping, consolidation to the latest target, relative accumulation, central non-seekable rejection, stale-observation immunity) |
| PLAY-008 | Clean shutdown releases mpv handle | §19.7 | P46, P48 | teardown test | mpv_backend | unit | PARTIAL |
| PLAY-009 | Reuse same handle via `loadfile` for media switch | §19.4 | P48, P56 | reuse test | PlaybackManager | integration | VERIFIED |

FIX-PLAYBACK-001 reconciliation (2026-10-21): fixed inline Stage video rendering (PLAY-VIS-001/002/003/006). Root cause: `video_surface.cpp` bound the *window* framebuffer to mpv while passing the *item's* dimensions, so libmpv drew an item-sized rectangle at the WINDOW origin (purple region covering sidebar/content; black Stage video area; rendering escaping Stage). Rewrote `VideoSurfaceItem` as a `QQuickFramebufferObject` so mpv always receives an item-sized FBO — frames render inside Stage, nothing outside. Verified: `qg_video_render` (3200 bright px, time-pos 2.47s), `qg_video_surface_offset` (frames at item rect 6592, outside item 0, duration=3, position=0.2). PLAY-002 remains VERIFIED. Real-desktop screenshot validation BLOCKED (full app does not map under Xvfb — pre-existing GL init hang).

## Cinema / Fullscreen

| ID | Requirement | Design Source | Planned Prompt(s) | Planned Test/Validation | Implementation | Tests | Status |
|---|---|---|---|---|---|---|---|
|| CINEMA-001 | In-window Cinema layout state | §9.1 | P54 | QML test | `Main.qml` | QML test | VERIFIED (`tst_cinema`: enter/exit, chrome collapse/restore, stage fills window, session preserved) — the window-state half of Cinema is tracked separately as CINEMA-006 |
|| CINEMA-002 | Stage↔Cinema continuity, no reload/reparent | §9.1, §19.5, §25.7 | P54, P58 | continuity test | PlaybackController | integration | VERIFIED (`PlaybackController::enterCinema` gated on `hasActiveSession()`; no reload/reparent; same `PlaybackSession` identity preserved) |
|| CINEMA-003 | Chrome/cursor auto-hide after 2s idle | §9.2, §25.7 | P54 | QML test | Cinema state | QML test | VERIFIED (`tst_cinema`: overlay MouseArea + timer; cursor BlankCursor after 2s, ArrowCursor on input; NOT hidden when `textEditing`) |
|| CINEMA-004 | Auto-advance keeps Cinema active | §9.2, §25.7 | P54, P56 | integration test | Cinema/context | integration | VERIFIED (Cinema derived from `cinemaActive` single source; `next()` through `PlaybackContext.step()` keeps session alive; no state reset) |
|| CINEMA-005 | `Esc`/`F11`/`F`/double-click toggle consistently | §9.2, §25.7 | P54 | QML/keyboard test | Cinema state | QML test | VERIFIED (`tst_cinema`: shortcuts exist, stage holds focus in Cinema (`stage.activeFocus` true), all routes through `PlaybackController.toggleCinema()` / `exitCinema()` / `escapePlayback()`) — all four routes now also drive the window state (CINEMA-006) |
|| CINEMA-006 | Cinema window-state transition: fullscreen the existing window on entry, restore the previous state on exit | §4.2, §4.4, §4.5, §25.7 | P54 | QML + desktop test | `Main.qml` window state | QML + X11 session | **IMPLEMENTED (static only, 2026-10-27 — FIX-STAGE-01 / AUD-010 half)**: `windowedVisibility` + `onCinemaActiveChanged` remember `window.visibility`, set `Window.FullScreen`, focus the Stage and wake both transport overlays on entry, and restore the remembered state (never a stale fullscreen) then focus the content view on exit; no playback call added, no second window. **TESTING DEFERRED:** X11 only (F11 removes decorations, Esc/exit restores the exact geometry, double-click toggles, no restart/re-buffer, pointer + overlays hide after 2 s); offscreen `Window.FullScreen` is a no-op, so the check asserts the `window.visibility` property. See `DEFERRED_TESTS.md` FIX-STAGE-01 / [[2026-10-27-fix-aud-010-cinema-fullscreen]] |

## Reel / Playback Context Panel

| ID | Requirement | Design Source | Planned Prompt(s) | Planned Test/Validation | Implementation | Tests | Status |
|---|---|---|---|---|---|---|---|
| REEL-001 | Ordered folder context list (same source as prev/next) | §10.1, §20.1 | P55 | order-equality test | `ReelModel`/`ReelPanel` | integration | VERIFIED (`qg_reel_wiring`: the Reel projection is the context list, identity-for-identity and in the same order). FIX-NAV-01 (2026-10-26) added the missing case: while a session plays from folder A and folder B is displayed, the context/Reel keep A's list and re-anchor when A is browsed back to — implemented/static, the runtime check is deferred, and `ReelPanel`'s header still reads the browsed folder name |
| REEL-002 | Current item anchored with highlight | §10.2 | P55, P58 | anchor test | ReelPanel | QML test | VERIFIED (highlight via the `reelIsCurrent` role asserted in `qg_reel_model`/`qg_reel_wiring`; `ReelPanel` scrolls the current row into view with `positionViewAtIndex` — the scroll itself is code-reviewed, not separately asserted) |
| REEL-003 | Filter All/Favorites/Unwatched constrains traversal | §10.2 | P55 | filter test | Reel/PlaybackContext | integration | VERIFIED (`qg_playback_context` filter lists; `qg_reel_wiring::filterConstrainsPrevNextAndReel`: filter shrinks the Reel and what Next traverses, re-anchoring by identity) |
| REEL-004 | Footer prev/next + favorite toggle | §10.2 | P55, P56 | QML test | ReelPanel | QML test | VERIFIED (`tst_reel`: footer intents + enablement; `qg_reel_wiring`: prev/next act on the context. The footer favorite → `StateController::toggleFavorite(identity)` hop in `Main.qml` is wired but not separately asserted) |
| REEL-005 | Not an editor (no reordering in MVP) | §10.2 | P55 | review | ReelPanel | review | VERIFIED (review: `ReelPanel` has no reorder/drag affordance; rows only select) |

## File Operations

| ID | Requirement | Design Source | Planned Prompt(s) | Planned Test/Validation | Implementation | Tests | Status |
|---|---|---|---|---|---|---|---|
| FILEOP-001 | Inline rename preserves extension, updates in place | §11.3, §25.9 | P62 | rename test | `filesystem/operations` | integration | VERIFIED (extension preservation + rename reaching filesystem VERIFIED by `qg_card_actions::renameReachesTheFileSystem`; model updates in place with targeted `dataChanged(PathRole, NameRole)` and `PathRepository::renamePath` updates persistent mapping; `test_folder_model::updatePathAndNameEmitsTargetedDataChanged`, `test_state_controller::renamePathUpdatesMapping`, `qg_action_feedback::renameSuccessReportsServiceOutcome`) |
| FILEOP-002 | Rename validation: invalid names + collisions rejected | §11.3 | P08, P62 | validation test | operations/paths | unit/integration | PLANNED |
| FILEOP-003 | Trash undoable for toast window | §11.4, §17.7 | P60, P64 | undo test | trash/toasts | integration | VERIFIED (REMEDIATION-010: `ActionsController` emits `toastRequested(msg, "trash", path)`, `ToastHost::undoTapped` reaches `undoLastTrash()`, and `qg_action_feedback` round-trips Trash → Undo with the file restored in place; no Undo affordance is offered when the trashed location is unknown) |
|| FILEOP-004 | Permanent delete requires Shift+Delete + confirm naming count | §11.1, §25.9 | P65 | confirm test | dialogs | QML test | VERIFIED (`tst_overlays_dialogs::test_permanentDeleteConfirmsWhileTrashDoesNot`: Trash is confirm-free, Shift+Delete/context menu open `ConfirmDeleteDialog` with itemCount + firstItemName, and `confirmed()` now dispatches `ActionsController.deletePermanently([path])` — no placeholder toast; `FilesOperationsService::deletePermanentlySync` removes files/dirs and reports real success/failure) |
| FILEOP-005 | Bulk partial failure summarized (no modal per item) | §11.4, §25.9 | P64 | summary test | toasts/controller | integration | PLANNED |
| FILEOP-006 | Reveal in Folder via D-Bus | §17.6, §25.9 | P59, P65 | reveal test | operations | integration | PLANNED |
| FILEOP-007 | Open With via GIO/GDesktopAppInfo | §17.6, §25.9 | P59, P65 | open-with test | operations | integration | PLANNED |
| FILEOP-008 | Properties dialog (stat + QG metadata + limitation note) | §17.6 | P59, P65 | QML test | PropertiesDialog | QML test | VERIFIED (REMEDIATION-010: the menu opens `PropertiesDialog` populated by `ActionsController::fileInfo` — real size/mtime/permissions/owner/MIME — plus the canonical model's favorite/watched; `qg_action_feedback::fileInfoReportsRealStats` and `tst_overlays_dialogs::test_propertiesDialogShowsRealStatsAndFlags`) |
|| FILEOP-009 | Cut/Copy/Paste + drag & drop | §11.1 | P63 | clipboard/DnD test | operations/clipboard | integration | PARTIAL (`ActionsController::cut`/`copyToClipboard`/`paste` wired; `ContentView` DropArea + card `Drag.mimeData` added; keyboard shortcuts `Ctrl+X/C/V` added in Main.qml; `test_action_feedback::clipboardCutPasteMovesFile` / `clipboardCopyPasteCreatesDupe` PASS; cross-app clipboard and full DnD flow UNVERIFIED without desktop session) |
| FILEOP-010 | Context menu shape; `Add to Collection` disabled | §11.2 | P61 | QML test | context menu | QML test | VERIFIED (REMEDIATION-010: verbs first, dividers between groups, the destructive pair last and coral, `Add to Collection` disabled, single-item verbs hidden for multi-selection with an "N items" title row and bulk verbs retained; `tst_overlays_dialogs::test_menuGroupsVerbsBeforeDestructivePair` + `test_menuMultiSelectionShowsTitleAndBulkVerbsOnly`, `tst_card_actions::test_menuIsPopulatedAndFutureItemDisabled`) |
| FILEOP-011 | Action results produce minimal model diffs; state stays consistent | §11.3, §17.5, §21.4 | P66 | minimal-signal test | controller/model | integration | PLANNED |

## Media State / Persistence

| ID | Requirement | Design Source | Planned Prompt(s) | Planned Test/Validation | Implementation | Tests | Status |
|---|---|---|---|---|---|---|---|
| STATE-001 | Favorite keyed by identity | §21.3, §23.11 | P30 | persist test | `state_controller` | integration | VERIFIED (`qg_card_actions`: toggleFavorite writes SQLite by identity, the model role flips immediately, and a fresh controller+model over the same database reads it back after "restart") |
| STATE-002 | Watched + progress semantics (100% on watched) | §21.3, §23.12 | P30 | semantics test | StateController | integration | VERIFIED (`qg_card_actions::watchedSetsAndClearsProgress`: watched → progress 1.0 and `watched` in the DB; unwatched clears both) |
| STATE-003 | State survives rename + restart | §21.4, §25.8 | P29, P30 | rename/reopen test | identity/repo | integration | VERIFIED (restart VERIFIED by `qg_card_actions`: favorite/watched persist across a fresh model+controller; rename survival: identity key preserved, `PathRepository::renamePath` moves the mapping, `media_state` untouched; `test_state_controller::renamePathUpdatesMapping` proves the mapping moves and state is untouched) |
|| STATE-004 | Disconnected-drive state retained | §23.14, §25.8 | P68 | retention test | mounts/repo | integration | VERIFIED (disconnect event retains `media_state`; no DB deletion; playback stops gracefully; `test_composition::mountDisconnectDrivesMountsController` asserts `disconnected()` and boundary matching) |
| STATE-005 | Corrupt DB preserved; app starts fresh | §21.5, §25.8 | P06 | integrity test | database | integration | VERIFIED |
| PERSIST-001 | SQLite WAL schema (paths/media_state/thumbnails/recents/bookmarks/settings) | §21.1, §21.2 | P06 | migration test | `persistence/*` | integration | VERIFIED |
| PERSIST-002 | Numbered idempotent migrations + schema_version | §21.1 | P06 | migration test | database | integration | VERIFIED |
| PERSIST-003 | Intention-revealing repository methods | §21.1 | P06 | API review | repositories | review | VERIFIED |
| PERSIST-004 | Identity↔path mapping + reconciliation | §21.3, §21.4 | P29 | reconciliation test | repositories (paths table; reconciliation P29) | integration | VERIFIED (`test_identity_map`: `renamePath` moves mapping, `reconcileSnapshot` adopts external moves, vanished identities retained within grace; `test_state_controller::renamePathUpdatesMapping` proves rename via controller updates the mapping) |
| PERSIST-005 | Per-folder sort/density settings persistence | §21.3 | P31 | reopen test | settings repo (wiring P31) | integration | VERIFIED |
| PERSIST-006 | Short transactions; per-thread connections; no unsafe sharing | §21.5 | P06 | concurrency test | database | integration | VERIFIED |
| PERSIST-007 | XDG data/cache/state locations | §21.1 | P02, P06 | path test | `src/config.cpp` | unit | VERIFIED |

## Keyboard & Accessibility

| ID | Requirement | Design Source | Planned Prompt(s) | Planned Test/Validation | Implementation | Tests | Status |
|---|---|---|---|---|---|---|---|
| KEY-001 | Focus/selection/playback independent | §13.1, §25.10 | P35, P36 | state test | SelectionState/ContentView | QML test | VERIFIED (FIX-SEL-01 2026-10-25: focus/selection are separate notifying properties; `clear()` keeps the cursor; rendered probe shows focus on row 0 while row 1 is selected; C++ probe `probe_fixed`/`probe_prefix_control` control pair; **§13.1's playback-independence clause ("playback continues while you browse elsewhere") was NOT covered by those tests and was broken until FIX-NAV-01 (2026-10-26)** — navigation stopped playback — so that half is implemented/static only, runtime check deferred) |
| KEY-002 | Complete §13.2 shortcut map (+ documented conflicts) | §13.2 | P36, P51 | keyboard test | shortcuts | QML test | PLANNED |
| KEY-003 | Type-to-select 700ms buffer | §13.3, §25.2 | P37 | type-to-select test | ContentView | QML test | PLANNED |
| KEY-004 | Focus movement (arrows/Page/Home/End, Tab order) | §13.3 | P36 | focus test | ContentView | QML test | PARTIAL (FIX-SEL-01 2026-10-25: arrows/PageUp/PageDown/Home/End move a clamped, visible cursor and Shift+arrows extend the range from the anchor; **Tab order** out of the content surface is still unwired) |
| KEY-005 | `Space` context sensitivity (select vs play/pause) | §13.2 | P36, P51 | keyboard test | shortcuts | QML test | PLANNED |
| KEY-006 | Text fields suppress single-key shortcuts | §13.2 | P36, P42, P62 | keyboard test | shortcuts/text fields | QML test | PLANNED |
| KEY-007 | Every §13 action keyboard-operable | §13.4, §25.10 | P38 | a11y test | QML controls | QML test | PLANNED |

## Performance

| ID | Requirement | Design Source | Planned Prompt(s) | Planned Test/Validation | Implementation | Tests | Status |
|---|---|---|---|---|---|---|---|
| PERF-001 | 50k-entry dir first screenful ≤300ms (SSD) | §22.6, §25.11 | P69 | benchmark | enumerator/model | performance | PLANNED |
| PERF-002 | Rapid 20-folder nav shows only final folder (no stale flashes) | §22.5, §25.11 | P27, P69 | nav-staleness test | tokens/model | integration | PLANNED |
| PERF-003 | 1,000-file bulk move never starves visible thumbnails | §22.2, §25.11 | P25, P69 | priority test | pools/scheduler | performance | PLANNED |
| PERF-004 | Bounded memory for 100k-entry directory | §22.3, §25.11 | P69 | memory benchmark | model/LRU/caps | performance | PLANNED |
| PERF-005 | No GUI-thread disk access or decoding | §22.1 | P04, P24, P28 | thread audit | services/workers | review/perf | PARTIAL (AUD-003 **FIXED 2026-10-25**: file operations are now on the I/O pool and `qg_file_ops_async` measures the GUI thread's block time directly — pre-fix 818 ms on a 3 MB / 400-file copy, post-fix the `Q_INVOKABLE` returns in microseconds and the watchdog keeps ticking. **Not yet covered:** the probe path (AUD-021) and thumbnail PNG encode (AUD-008) still touch the GUI thread) |

## Native Linux

| ID | Requirement | Design Source | Planned Prompt(s) | Planned Test/Validation | Implementation | Tests | Status |
|---|---|---|---|---|---|---|---|
| LINUX-001 | X11/xcb primary target works without child-window embedding | §25.12 | P02, P47, P54 | X11 run | main (surface/playback P47) | manual | PARTIAL |
| LINUX-002 | Removable drives appear/disappear without restart; eject where supported | §17.4, §23.13, §25.12 | P11, P15, P68 | mount-change test | `mounts`/`PlacesSidebar` | integration | VERIFIED for appear/disappear + capability gating (FIX-UI-003: `disconnectedVolumeRetainedAndReconnected`, `disconnectedVolumeAgesOut`, `realHostClassificationReport`; eject/unmount affordance appears only when GVfs/UDisks2 reports can_eject/can_unmount, and the operation refuses system mounts). Real eject operation UNVERIFIED — no authorized disposable device. |
| LINUX-003 | Permission errors produce access-denied state (no crash) | §12, §25.12 | P67 | permission test | StateOverlay | integration | PARTIAL (REMEDIATION-010: a browser error whose message indicates denial maps to the `accessDenied` overlay whose action copies the path; the signal is injected in tests — no fixture produces it from a real EACCES directory) |
| LINUX-004 | High-DPI/fractional scaling renders crisply | §25.12 | P70 | scale test | Theme/QML | QML test | PLANNED |
| LINUX-005 | XDG base directories followed | §21.1, §25.12 | P02 | path test | `src/config.cpp` | unit | VERIFIED |
| LINUX-006 | Native/portal dialogs where available | §25.12 | P65 | dialog test | dialogs | integration | PLANNED |
|| LINUX-007 | Drive disconnect during playback fails gracefully | §23.14 | P68 | disconnect test | mounts/playback | integration | VERIFIED (`test_composition::mountDisconnectDrivesMountsController`: disconnect event sets `disconnected()` true, `isInsideDisconnectedVolume()` matches path boundary, and `composition.cpp` lambda connects `disconnectedChanged` → `PlaybackController::stop()` when session path is inside the disconnected volume; control case: different volume does NOT stop) |

## Quality

| ID | Requirement | Design Source | Planned Prompt(s) | Planned Test/Validation | Implementation | Tests | Status |
|---|---|---|---|---|---|---|---|
| QUAL-001 | ASan/UBSan builds; no QG-introduced leaks/resource defects | §24 (Phase 9), §15.4 | P71 | sanitizer runs | build/testing | sanitizer | PLANNED |

## Packaging

| ID | Requirement | Design Source | Planned Prompt(s) | Planned Test/Validation | Implementation | Tests | Status |
|---|---|---|---|---|---|---|---|
| PKG-001 | Desktop entry `.desktop` installed | §16.1 | P72 | `desktop-file-validate` | `packaging/*.desktop` | validation | PLANNED |
| PKG-002 | AppStream metainfo | §16.1 | P72 | `appstreamcli validate` | `packaging/*.metainfo.xml` | validation | PLANNED |
| PKG-003 | Native `.deb` primary release artifact | §16.1 | P73 | `dpkg-deb -I/-c` | `packaging/debian` | package build | PLANNED |
| PKG-004 | Runtime dependency declarations | §16.1 | P73 | metadata inspection | debian/CPack | package build | PLANNED |
| PKG-005 | Optional AppImage secondary artifact | §16.1 | P74 | AppImage build/smoke | `packaging/appimage` | build | PLANNED |
| PKG-006 | Icons/resources installed to correct locations | §16, §16.1 | P72 | contents inspection | resources/install rules | validation | PLANNED |

---

Matrix entries: 146. Reconciled 2026-10-08: requirements covered by validated P01–P21 prompts are `VERIFIED`; requirements whose code exists but whose full gate/checkpoint has not yet run are `IMPLEMENTED`; requirements with a partial prompt set complete are `PARTIAL`; everything from P22 onward remains `PLANNED`. No requirement is `BLOCKED`.