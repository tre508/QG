---
type: architecture
project: QG
created: 2026-10-20
updated: 2026-10-20
status: verified
tags:
  - architecture
  - components
  - controllers
  - models
  - services
source:
  - DESIGN.md
  - ARCHITECTURE_CURRENT.md
  - src/controllers/
  - src/models/
  - src/services/
---

# QG Component Map

## Controllers (GUI Thread)

| Controller | File | Responsibility | Key Dependencies |
|------------|------|----------------|------------------|
| **BrowserController** | `src/controllers/browser_controller.cpp` | Navigation stack, enumeration, watcher, model population, per-folder settings | FolderModel, FilesystemEnumerator, DirectoryWatcher, SettingsRepository |
| **SearchController** | `src/controllers/search_controller.cpp` | Query lifecycle, debounce, scope, proxy wiring, result summary | SearchProxy, RecursiveSearchService, RecursiveSearchModel |
| **PlaybackController** | `src/controllers/playback_controller.cpp` | PlaybackManager bridge, ordered context, Stage/Cinema state, Reel wiring | PlaybackManager, PlaybackContext, FolderModel, ReelModel |
| **ActionsController** | `src/controllers/actions_controller.cpp` | File ops (rename, trash, move, copy, reveal, open-with, properties), toasts, undo | FileOperationsService, TrashService, clipboard state |
| **MountsController** | `src/controllers/mounts_controller.cpp` | Volume discovery, eject, disconnect handling, sidebar sync | MountService, PlacesModel |
| **StateController** | `src/controllers/state_controller.cpp` | Favorite/Watched toggles, identity-keyed, DB write-through | MediaStateRepository, FolderModel |
| **ThumbnailController** | `src/controllers/thumbnail_controller.cpp` | Validated delivery, visible-first scheduling, retry | ThumbnailService, FolderModel |
| **ProbeController** | `src/controllers/probe_controller.cpp` | Validated delivery, coalesced resort hook (200ms) | MediaProbeService, FolderModel, BrowserController |
| **KeyboardFramework** | `src/controllers/keyboard_framework.cpp` | Central shortcut router, text-entry suppression | BrowserController, PlaybackController, SelectionState, FolderModel |
| **TypeToSelectBuffer** | `src/controllers/type_to_select_buffer.cpp` | 700ms timeout, printable key buffer | ContentView |

## Models (GUI Thread Ownership)

| Model | File | Role | Key Roles/Methods |
|-------|------|------|-------------------|
| **FolderModel** | `src/models/folder_model.cpp` | Canonical ordered entry list (folders + videos) | 14 roles (Identity, Path, Name, Kind, Thumbnail, Duration, Size, Mtime, Favorite, Watched, Progress, Width, Height, ThumbnailState); applySnapshot/applyDiff/reorder; entriesMatching projection hook |
| **SearchProxy** | `src/models/search_proxy.cpp` | Filter over FolderModel for current query | QSortFilterProxyModel subclass; match-position roles for Manifest highlight |
| **ReelModel** | `src/models/reel_model.cpp` | Videos-only projection + playlist position | Derived from FolderModel via filter; currentIndex, prev/next |
| **PlacesModel** | `src/models/places_model.cpp` | Sidebar groups: pinned, XDG places, devices, recents | ListModel; setRecent/setVolumes |
| **SelectionState** | `src/models/selection_state.cpp` | Selected identities, focus index, range anchor | QtObject; selectOne/toggle/extendRange/clear; selectedIdentities() |
| **RecursiveSearchModel** | `src/models/recursive_search_model.cpp` | Recursive search results with full metadata | 11 roles (Duration, Size, Mtime, Favorite, Watched, Progress, Width, Height, Thumbnail, ThumbnailState, Identity) |

## Services (Worker-Safe)

| Service | File | Responsibility | Thread Pool |
|---------|------|----------------|-------------|
| **FilesystemEnumerator** | `src/filesystem/enumerator.cpp` | readdir/fstatat scan, filtered batches (~200), TaskToken | I/O |
| **FileOperationsService** | `src/filesystem/operations.cpp` | Rename (collision guard), Move/Copy (chunked), Reveal (D-Bus), Open With (GIO) | I/O |
| **TrashService** | `src/filesystem/trash.cpp` | gio trash preferred, freedesktop fallback, undo | I/O |
| **MountService** | `src/filesystem/mounts.cpp` | /proc/self/mountinfo parse, GIO/UDisks2 enrichment hooks, 5s poll | I/O |
| **DirectoryWatcher** | `src/filesystem/watcher.cpp` | QFileSystemWatcher, 300ms debounce, identity diff | GUI (Qt) |
| **MediaProbeService** | `src/media/probe.cpp` | In-process FFmpeg, duration/resolution/rotation, cached by identity+mtime/size | CPU/Media |
| **ThumbnailService** | `src/media/thumbnails.cpp` | Freedesktop cache first, FFmpeg extraction, visible-first, LRU, retries | CPU/Media |
| **PersistenceService** | `src/persistence/database.cpp` + repositories | SQLite WAL, repositories per table, migrations | GUI (per-thread DB) |
| **SortService** | `src/sorting/sort_service.cpp` | Deterministic total order, folders-first, locale-aware | GUI (called from BrowserController) |
| **SearchService** | `src/search/search_service.cpp` | Current-dir filter/ranking, casefold Unicode, word-boundary boost | GUI |

## QML Components (Presentation Only)

| Component | File | Responsibility |
|-----------|------|----------------|
| **Main.qml** | `qml/QG/Main.qml` | ApplicationWindow, 6 regions, contentState, keyboard map, dialogs |
| **PathBar.qml** | `qml/QG/PathBar.qml` | Nav buttons, breadcrumbs, search morph, order/density/overflow menus |
| **PlacesSidebar.qml** | `qml/QG/PlacesSidebar.qml` | Rail/panel, Pinned/Places/Devices/Recent, drag-to-pin, eject |
| **ContentView.qml** | `qml/QG/ContentView.qml` | GridView/ListView/Stage host, visibleRangeChanged → schedulers |
| **VideoCard.qml** | `qml/QG/delegates/VideoCard.qml` | Fixed badges, playing ribbon, watched progress, hover actions, inline rename |
| **FolderCard.qml** | `qml/QG/delegates/FolderCard.qml` | 2×2 mosaic thumbnail, count chip, hover enter, drop target |
| **CompactRow.qml** | `qml/QG/delegates/CompactRow.qml` | 96×54 thumb, name, duration, size, date, badges |
| **ManifestRow.qml** | `qml/QG/delegates/ManifestRow.qml` | Columnar: thumb, name+path (highlight), duration, size, modified, state, actions |
| **Stage.qml** | `qml/QG/playback/Stage.qml` | Inline player, bloom geometry, cinema fill, transport + scrim |
| **TransportBar.qml** | `qml/QG/playback/TransportBar.qml` | Play/pause, prev/next, ±10, seek, volume, fullscreen, close |
| **ReelPanel.qml** | `qml/QG/playback/ReelPanel.qml` | Ordered list, current highlight, filter, footer prev/next/fav |
| **ContextStrip.qml** | `qml/QG/strips/ContextStrip.qml` | Folder name, counts, order label, density selector |
| **StatusStrip.qml** | `qml/QG/strips/StatusStrip.qml` | Selection summary, free space, busy count, transient text |
| **StateOverlay.qml** | `qml/QG/overlays/StateOverlay.qml` | Empty/access-denied/disconnected/error/no-results |
| **ToastHost.qml** | `qml/QG/overlays/ToastHost.qml` | Bottom-center, 4s auto-dismiss, Undo affordance |
| **Dialogs** | `qml/QG/dialogs/*.qml` | Properties, RenameConflict, ConfirmDelete |
| **Theme.qml** | `qml/QG/Theme.qml` | Design tokens, HC bound to platform, RM app-toggle |
| **Icon.qml** | `qml/QG/Icon.qml` | MultiEffect tinting, 16 monoline SVGs |

## CoreServices (Single Ownership Root)

`src/core/composition.cpp` — `CoreServices` class owns and wires all non-UI subsystems:
- StateController, ThumbnailController, ProbeController
- SearchProxy, SearchController
- FileOperationsService, TrashService, ActionsController
- MountsController
- ReelModel, SelectionState, TypeToSelectBuffer
- MpvBackend, PlaybackManager, PlaybackContext, PlaybackController
- KeyboardFramework

Wired once in `App::initialize()` — no duplicate subsystems.