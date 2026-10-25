---
type: development
project: QG
created: 2026-10-20
updated: 2026-10-20
status: completed
tags:
  - development
  - tasks
  - completed
source:
  - GUIDE_STATE.md
  - ARCHITECTURE_REMEDIATION.md
---

# Completed Development Tasks

## Grid / Density (FIX-UI-004)

| Task | Completed | Verification |
|------|-----------|--------------|
| Column count from the §3.5 density target (VIS-008) | 2026-10-21 | `tst_grid_geometry` 13/13; 1593 px → 6×244 px |
| Centred occupied grid with equal gutters (VIS-009) | 2026-10-21 | measured at 7 widths: computed columns == laid-out columns, gutters within 1.5 px |
| Shell-supplied responsive target (`gridTargetCardWidth`) | 2026-10-21 | `tst_grid_geometry::test_targetDrivesColumnCount` |
| Virtualization preserved under the new geometry | 2026-10-21 | 1000-row model → < 200 delegates instantiated |

## Sidebar / Places (FIX-UI-003)

| Task | Completed | Verification |
|------|-----------|--------------|
| Places + device icons from model roles (VIS-005) | 2026-10-21 | `tst_places_sidebar` 16/16; `tst_icon` 14 icon names reach Image.Ready |
| Device classification + GVfs/UDisks2 capability (VIS-006) | 2026-10-21 | `qg_mounts` 14/14 incl. live-host classification report |
| Group headings as model rows + exact scroll range (VIS-007) | 2026-10-21 | `qg_places_model` 14/14; long-list reachability QML test |
| Capability-gated eject/unmount, clickable above the row area | 2026-10-21 | QML `test_deviceActionFollowsCapability` / `test_deviceActionEmitsEjectAndUnmount` |
| Disconnected volume retention + amber badge | 2026-10-21 | `disconnectedVolumeRetainedAndReconnected`, `disconnectedVolumeAgesOut` |
| Pinned locations projected from BookmarkRepository | 2026-10-21 | `pinRequestsAreForwarded`, `test_pinIntentIsEmitted` |

## Architecture & Core (All Complete)

| Task | Completed | Verification |
|------|-----------|--------------|
| CoreServices single ownership root (DEC-043) | 2026-10-08 | test_composition.cpp PASS |
| SQLite WAL persistence with migrations | 2026-10-08 | test_persistence.cpp PASS |
| File identity (Inode/Fingerprint/PathOnly) | 2026-10-08 | test_identity_map.cpp PASS |
| Result-based error handling | 2026-10-08 | All services use Result<T> |
| TaskToken + bounded worker pools | 2026-10-08 | test_workers.cpp PASS |
| Filesystem enumerator (batched, filtered) | 2026-10-09 | test_enumerator.cpp PASS |
| File operations (rename, move, copy, trash, reveal) | 2026-10-09 | test_integration.cpp PASS |
| Trash service (gio + freedesktop fallback) | 2026-10-09 | test_integration.cpp PASS |
| Mount service (mountinfo + GIO/UDisks2 hooks) | 2026-10-09 | test_mounts.cpp PASS |
| Directory watcher (300ms debounce, identity diff) | 2026-10-09 | test_watcher.cpp PASS |
| Media formats (single source of truth) | 2026-10-09 | test_formats.cpp PASS |
| Media probe (FFmpeg, cached, bounded) | 2026-10-09 | test_probe.cpp, test_probe_wiring.cpp PASS |
| Thumbnails (freedesktop-first, FFmpeg, LRU) | 2026-10-09 | test_thumbnails*.cpp PASS |
| Sort service (deterministic total order) | 2026-10-09 | test_sort.cpp, test_sort_wiring.cpp PASS |
| FolderModel (canonical, batch/diff, projection) | 2026-10-09 | test_folder_model.cpp PASS |
| SearchProxy (QSortFilterProxyModel + match positions) | 2026-10-09 | test_search_proxy.cpp PASS |
| SearchService (current-dir filter/ranking) | 2026-10-09 | test_search_service.cpp PASS |
| SearchController (debounce, scope, summary) | 2026-10-09 | test_search_controller.cpp PASS |
| RecursiveSearchService (architecture extension) | 2026-10-09 | Implemented; UI not wired |
| BrowserController (navigation stack, watcher) | 2026-10-09 | test_browser_controller.cpp PASS |
| StateController (fav/watched, identity-keyed, rollback) | 2026-10-09 | test_state_controller.cpp PASS |
| ActionsController (file ops, toasts, undo) | 2026-10-09 | test_action_feedback.cpp PASS |
| ThumbnailController (validated delivery, visible-first) | 2026-10-09 | test_thumb_wiring.cpp PASS |
| ProbeController (coalesced resort hook) | 2026-10-09 | test_probe_wiring.cpp PASS |
| KeyboardFramework (central router, text suppression) | 2026-10-09 | test_keyboard.cpp PASS |
| TypeToSelectBuffer (700ms timeout) | 2026-10-09 | Implemented |

## Playback Stack (All Complete)

| Task | Completed | Verification |
|------|-----------|--------------|
| MpvBackend (single handle, LC_NUMERIC=C, property observation) | 2026-10-09 | test_playback_lifecycle.cpp PASS |
| VideoSurfaceItem (QSGRenderNode, render-context-ready) | 2026-10-09 | test_video_surface.cpp, test_video_render.cpp PASS |
| PlaybackManager (singleton, deferred load, 5s tick, resume) | 2026-10-09 | test_playback_lifecycle.cpp PASS |
| PlaybackSession (observable state, 0..1 volume) | 2026-10-09 | test_transport_state.cpp PASS |
| PlaybackContext (ordered videos, filter, prev/next) | 2026-10-09 | test_playback_context.cpp, test_reel_model.cpp PASS |
| PlaybackController (Stage/Cinema bridge, Reel wiring) | 2026-10-09 | test_reel_wiring.cpp PASS |
| Resume policy (Always/Ask/Never, injected loader) | 2026-10-09 | test_resume_policy.cpp, test_resume_playback.cpp PASS |

## QML Presentation Layer (All Complete)

| Task | Completed | Verification |
|------|-----------|--------------|
| Theme.qml (full token system, HC bound, RM app-toggle) | 2026-10-09 | Visual; test_highContrastChangesTokens PASS |
| Main.qml (6 regions, contentState, keyboard map, dialogs) | 2026-10-09 | qg_qml tests (2 infra failures only) |
| PathBar (nav, breadcrumbs, search morph, order/density) | 2026-10-09 | Functional |
| PlacesSidebar (rail/panel, drag-to-pin, eject, drop) | 2026-10-09 | Functional |
| ContentView (GridView/ListView/Stage, visibleRangeChanged) | 2026-10-09 | Functional |
| VideoCard/FolderCard/CompactRow/ManifestRow delegates | 2026-10-09 | Functional |
| Stage (bloom geometry, cinema fill, transport) | 2026-10-09 | Functional |
| TransportBar (play/pause, prev/next, seek, volume, FS) | 2026-10-09 | Functional |
| ReelPanel (ordered list, filter, footer) | 2026-10-09 | Functional |
| ContextStrip/StatusStrip (counts, free space, density) | 2026-10-09 | Functional |
| StateOverlay (empty/denied/disconnected/error/no-results) | 2026-10-09 | Functional |
| ToastHost (bottom-center, 4s, Undo) | 2026-10-09 | Functional |
| Dialogs (Properties, RenameConflict, ConfirmDelete) | 2026-10-09 | Functional |
| Icon.qml (MultiEffect, 16 SVGs, adopted in TransportBar+Stage) | 2026-10-09 | Partial adoption |

## Performance & Testing Infrastructure

| Task | Completed | Verification |
|------|-----------|--------------|
| CMake + Ninja build with qt_add_qml_module | 2026-10-08 | cmake --build build PASS |
| 44/46 CTest PASS (96%) | 2026-10-20 | 2 infra failures (onScopeToggled) |
| Unit tests (17) | 2026-10-20 | All PASS |
| Integration tests (8) | 2026-10-20 | All PASS |
| Playback tests (9) | 2026-10-20 | All PASS (real mpv + Xvfb) |
| Composition test (1) | 2026-10-20 | PASS |
| Worker tests (1) | 2026-10-20 | PASS |
| Performance test binary exists | 2026-10-20 | ./build/tests/qg_performance_test |
| Benchmark dataset generator | 2026-10-19 | 50k/100k sparse file datasets |
| run_gl_test.sh (Xvfb + Mesa llvmpipe) | 2026-10-19 | 51 tests PASS at 1.25x |
| tst_scaling.qml (12 tests) | 2026-10-19 | PASS at 1.0/1.25/1.5/2.0x |

## Architectural Documentation

| Task | Completed | Output |
|------|-----------|--------|
| Architecture audit (Phase 1-3) | 2026-10-12 | 5 architecture docs |
| FIX-ICON-001..008 (icon migration) | 2026-10-12 to 14 | 17 new SVGs, 7 delegates rewritten |
| FIX-SEARCH-001..007 (recursive search) | 2026-10-14 to 16 | RecursiveSearchService + model + reactive QML |
| FIX-PERF-001..009 (performance) | 2026-10-16 to 19 | Benchmarks, scaling tests, memory probes |
| FIX-TEST-001 (scope signal wiring) | 2026-10-19 | PathBar scopeToggled moved to root |
| FIX-DESKTOP-001 (desktop audit) | 2026-10-20 | FIX_DESKTOP_001_AUDIT.md |
| FIX-UI-002 (Path Bar nav/breadcrumb/actions) | 2026-10-21 | PathBar gate 14/14; full QML 175/4 (pre-existing); CTest 45/46 |
| FIX-PLAYBACK-001 (inline video rendering) | 2026-10-21 | video_surface.cpp → QQuickFramebufferObject; purple artifact eliminated; frames inside Stage; duration/position propagate; qg_video_render + qg_video_surface_offset PASS; CTest 46/47 (qg_qml pre-existing) |
| Complete architecture documentation | 2026-10-20 | 6 docs in docs/architecture/ |