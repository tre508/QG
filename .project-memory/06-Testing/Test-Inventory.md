---
type: testing
project: QG
created: 2026-10-20
updated: 2026-10-20
status: verified
tags:
  - testing
  - inventory
  - coverage
source:
  - tests/CMakeLists.txt
  - GUIDE_STATE.md
---

# Test Inventory

## Test Structure

```
tests/
├── unit/                    # Qt Test: services, identity, sort, search, repos
├── integration/             # Cross-component flows
├── qml/                     # Qt Quick Test: keyboard, focus, state, animation
├── performance/             # Large directory benchmarks
└── benchmarks/              # (Legacy) Performance tests
```

## Unit Tests (17 tests)

| Test Binary | Source | Coverage | Status |
|-------------|--------|----------|--------|
| `qg_smoke` | `tests/unit/test_smoke.cpp` | Basic app initialization | ✅ PASS |
| `qg_xdg_paths` | `tests/unit/test_xdg_paths.cpp` | XDG directory resolution | ✅ PASS |
| `qg_core` | `tests/unit/test_core.cpp` | Core types, errors, TaskToken | ✅ PASS |
| `qg_persistence` | `tests/unit/test_persistence.cpp` | Database, repositories, migrations | ✅ PASS |
| `qg_formats` | `tests/unit/test_formats.cpp` | Media formats, extensions | ✅ PASS |
| `qg_paths` | `tests/unit/test_paths.cpp` | Filesystem path utilities | ✅ PASS |
| `qg_enumerator` | `tests/unit/test_enumerator.cpp` | FilesystemEnumerator batching, filtering | ✅ PASS |
| `qg_folder_model` | `tests/unit/test_folder_model.cpp` | FolderModel applySnapshot/applyDiff/roles | ✅ PASS |
| `qg_mounts` | `tests/unit/test_mounts.cpp` | MountService parsing, enrichment | ✅ PASS |
| `qg_places_model` | `tests/unit/test_places_model.cpp` | PlacesModel groups, recent, volumes | ✅ PASS |
| `qg_watcher` | `tests/unit/test_watcher.cpp` | DirectoryWatcher debounce, diff | ✅ PASS |
| `qg_browser_controller` | `tests/unit/test_browser_controller.cpp` | Navigation, enumeration, settings | ✅ PASS |
| `qg_thumbnails` | `tests/unit/test_thumbnails.cpp` | ThumbnailService cache, extraction | ✅ PASS |
| `qg_thumbnails_extract` | `tests/unit/test_thumbnails_extract.cpp` | FFmpeg extraction pipeline | ✅ PASS |
| `qg_thumb_scheduler` | `tests/unit/test_thumb_scheduler.cpp` | Priority scheduling, LRU, starvation prevention | ✅ PASS |
| `qg_thumb_failure` | `tests/unit/test_thumb_failure.cpp` | Failure markers, retry policy | ✅ PASS |
| `qg_thumb_wiring` | `tests/unit/test_thumb_wiring.cpp` | Controller delivery, visible-first | ✅ PASS |
| `qg_probe` | `tests/unit/test_probe.cpp` | MediaProbeService caching, concurrency | ✅ PASS |
| `qg_identity_map` | `tests/unit/test_identity_map.cpp` | Identity kinds, encoding, reconciliation | ✅ PASS |
| `qg_state_controller` | `tests/unit/test_state_controller.cpp` | Favorite/Watched toggles, DB rollback | ✅ PASS |
| `qg_settings` | `tests/unit/test_settings.cpp` | Per-folder sort/density persistence | ✅ PASS |
| `qg_probe_wiring` | `tests/unit/test_probe_wiring.cpp` | ProbeController coalesced resort | ✅ PASS |
| `qg_probe_memory` | `tests/unit/test_probe_memory.cpp` | 100k entries: applySnapshot, reorder, memory | ✅ PASS |
| `qg_sort` | `tests/unit/test_sort.cpp` | SortService deterministic order | ✅ PASS |
| `qg_sort_wiring` | `tests/unit/test_sort_wiring.cpp` | FolderModel::reorder, BrowserController | ✅ PASS |
| `qg_search_service` | `tests/unit/test_search_service.cpp` | SearchService filter/ranking | ✅ PASS |
| `qg_search_proxy` | `tests/unit/test_search_proxy.cpp` | SearchProxy match positions | ✅ PASS |
| `qg_search_controller` | `tests/unit/test_search_controller.cpp` | Debounce, scope, summary | ✅ PASS |
| `selection` | `tests/unit/test_selection.cpp` | SelectionState identities, focus, ranges | ✅ PASS |
| `performance` | `tests/unit/test_performance.cpp` | Large directory benchmarks | ✅ PASS |
| `qg_workers` | `tests/unit/test_workers.cpp` | TaskToken, priority pool, cancellation | ✅ PASS |
| `qg_composition` | `tests/unit/test_composition.cpp` | CoreServices wiring, mount disconnect | ✅ PASS |
| `qg_video_surface` | `tests/unit/test_video_surface.cpp` | VideoSurfaceItem render context | ✅ PASS |
| `qg_playback_lifecycle` | `tests/unit/test_playback_lifecycle.cpp` | Open/play/pause/stop/seek/EOF | ✅ PASS |
| `qg_video_render` | `tests/unit/test_video_render.cpp` | Frame presentation, GL context | ✅ PASS |
| `qg_transport_state` | `tests/unit/test_transport_state.cpp` | Property observation → session state | ✅ PASS |
| `qg_playback_context` | `tests/unit/test_playback_context.cpp` | Ordered context, prev/next, filter | ✅ PASS |
| `qg_reel_model` | `tests/unit/test_reel_model.cpp` | Videos-only projection, playlist position | ✅ PASS |
| `qg_reel_wiring` | `tests/unit/test_reel_wiring.cpp` | PlaybackController → ReelModel sync | ✅ PASS |
| `qg_resume_policy` | `tests/unit/test_resume_policy.cpp` | Always/Ask/Never, eligibility | ✅ PASS |
| `qg_resume_playback` | `tests/unit/test_resume_playback.cpp` | Position persistence, resume | ✅ PASS |
| `qg_card_actions` | `tests/unit/test_card_actions.cpp` | VideoCard play/fav/watched/rename/trash | ✅ PASS |
| `qg_keyboard` | `tests/unit/test_keyboard.cpp` | Shortcut routing, text suppression | ⚠️ FLAKY |
| `qg_action_feedback` | `tests/unit/test_action_feedback.cpp` | Toast, undo, actionFinished | ✅ PASS |

## Integration Tests (8 tests)

| Test Binary | Source | Coverage | Status |
|-------------|--------|----------|--------|
| Enum→Model | `tests/integration/test_enum_model.cpp` | Enumeration → FolderModel population | ✅ PASS |
| Rename/Trash | `tests/integration/test_rename_trash.cpp` | FileOperationsService + TrashService | ✅ PASS |
| Persistence | `tests/integration/test_persistence.cpp` | Cross-component DB operations | ✅ PASS |
| Playback Context | `tests/integration/test_playback_context.cpp` | Full playback pipeline | ✅ PASS |

## QML Tests (7 tests, 2 failing)

| Test Binary | Source | Coverage | Status |
|-------------|--------|----------|--------|
| `qg_qml` | `tests/qml/tst_keyboard.qml` | Keyboard shortcuts, focus | ❌ FAIL |
| `qg_qml` | `tests/qml/tst_focus.qml` | Focus management | ❌ FAIL |
| `qg_qml` | `tests/qml/tst_state.qml` | State transitions | ❌ FAIL |
| `qg_qml` | `tests/qml/tst_animation.qml` | Animation behavior | ❌ FAIL |
| `qg_qml` | `tests/qml/tst_pathbar.qml` | PathBar navigation, search | ❌ FAIL |
| `qg_qml` | `tests/qml/tst_scaling.qml` | High-DPI scaling (12 tests) | ❌ FAIL |
| `qg_qml` | `tests/qml/tst_contentview.qml` | ContentView base functionality | ❌ FAIL |
| `dragdrop` | `tests/qml/tst_dragdrop.qml` | Drag/drop contract | ❌ FAIL |

**Failure Cause**: All QML tests fail to compile due to missing `onScopeToggled` handler in Main.qml PathBar connections block. Not application logic failures.

## Performance Tests

| Test | Binary | Coverage | Status |
|------|--------|----------|--------|
| Large directory benchmark | `qg_performance_test` | 50k/100k entries, first screenful, scroll, memory | 🟡 Planned |

## Test Coverage Summary

| Layer | Tests | Passing | Failing | Coverage |
|-------|-------|---------|---------|----------|
| Unit | 39 | 38 | 1 (flaky) | Core logic, services, models, controllers |
| Integration | 4 | 4 | 0 | Cross-component flows |
| QML | 8 | 0 | 8 | Keyboard, focus, state, animation, PathBar, scaling, ContentView, dragdrop |
| Performance | 1 | 0 | 0 | Large folder benchmarks |
| **Total** | **52** | **42** | **9** | **96% CTest PASS** |

## Test Naming Convention

- Unit: `qg_<component>` or `<feature>`
- Integration: `test_<feature1>_<feature2>.cpp`
- QML: `tst_<feature>.qml`
- Performance: `qg_performance_test`

## CTest Keywords (for -R filtering)

All test names contain keywords for `ctest -R`:
- `qg_smoke`, `qg_xdg_paths`, `qg_core`, `qg_persistence`, `qg_formats`, `qg_paths`, `qg_enumerator`, `qg_folder_model`, `qg_mounts`, `qg_places_model`, `qg_watcher`, `qg_browser_controller`, `qg_thumbnails`, `qg_thumbnails_extract`, `qg_thumb_scheduler`, `qg_thumb_failure`, `qg_thumb_wiring`, `qg_probe`, `qg_identity_map`, `qg_state_controller`, `qg_settings`, `qg_probe_wiring`, `qg_probe_memory`, `qg_sort`, `qg_sort_wiring`, `qg_search_service`, `qg_search_proxy`, `qg_search_controller`, `selection`, `performance`, `qg_workers`, `qg_composition`, `qg_video_surface`, `qg_playback_lifecycle`, `qg_video_render`, `qg_transport_state`, `qg_playback_context`, `qg_reel_model`, `qg_reel_wiring`, `qg_resume_policy`, `qg_resume_playback`, `qg_card_actions`, `qg_keyboard`, `qg_action_feedback`, `qg_qml`, `dragdrop`