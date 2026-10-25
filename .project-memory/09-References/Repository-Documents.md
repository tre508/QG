---
type: reference
project: QG
created: 2026-10-20
updated: 2026-10-20
status: verified
tags:
  - reference
  - documents
  - index
source:
  - /home/treshen/Projects/QG/
---

# Repository Documents Index

## Authoritative Specifications

| Document | Purpose | Location |
|----------|---------|----------|
| **DESIGN.md** | Complete architectural specification, requirements, behavior | `/home/treshen/Projects/QG/DESIGN.md` |
| **dev-guide.md** | Ordered implementation pipeline, work items | `/home/treshen/Projects/QG/dev-guide.md` |
| **GUIDE_STATE.md** | Execution progress ledger, task status | `/home/treshen/Projects/QG/GUIDE_STATE.md` |
| **DECISIONS.md** | Durable architecture/implementation decisions | `/home/treshen/Projects/QG/DECISIONS.md` |
| **REQUIREMENTS_MATRIX.md** | Requirement coverage tracking | `/home/treshen/Projects/QG/REQUIREMENTS_MATRIX.md` |
| **DEFERRED_TESTS.md** | Consolidates the tests deferred by the logic-first policy: pending register, known defects, batch checklist | `/home/treshen/Projects/QG/DEFERRED_TESTS.md` |

## Architecture Documentation

| Document | Purpose | Location |
|----------|---------|----------|
| **ARCHITECTURE_TARGET.md** | Intended final architecture per DESIGN.md | `docs/architecture/ARCHITECTURE_TARGET.md` |
| **ARCHITECTURE_CURRENT.md** | Actual implementation state with source refs | `docs/architecture/ARCHITECTURE_CURRENT.md` |
| **ARCHITECTURE_GAP_ANALYSIS.md** | Design-to-implementation comparison | `docs/architecture/ARCHITECTURE_GAP_ANALYSIS.md` |
| **ARCHITECTURE_REMEDIATION.md** | Dependency-ordered remediation plan | `docs/architecture/ARCHITECTURE_REMEDIATION.md` |
| **ARCHITECTURE_OVERVIEW.md** | Consolidated visual gap map + E2E flows | `docs/architecture/ARCHITECTURE_OVERVIEW.md` |
| **FIX_DESKTOP_001_AUDIT.md** | Desktop integration architecture audit | `docs/architecture/FIX_DESKTOP_001_AUDIT.md` |

## Architecture Diagrams (Archify-generated)

| Diagram | Format | Location |
|---------|--------|----------|
| Target Architecture | SVG, HTML | `docs/architecture/diagrams/target-architecture.svg` |
| Current Architecture | SVG, HTML | `docs/architecture/diagrams/current-architecture.svg` |
| Gap Map | SVG, HTML | `docs/architecture/diagrams/gap-map.svg` |
| End-to-End Flow | SVG, HTML | `docs/architecture/diagrams/e2e-flow.svg` |

## Build & Configuration

| File | Purpose | Location |
|------|---------|----------|
| **CMakeLists.txt** | Root build configuration | `/home/treshen/Projects/QG/CMakeLists.txt` |
| **CMakePresets.json** | CMake presets | `/home/treshen/Projects/QG/CMakePresets.json` |
| **src/CMakeLists.txt** | Source build config | `/home/treshen/Projects/QG/src/CMakeLists.txt` |
| **qml/CMakeLists.txt** | QML module build | `/home/treshen/Projects/QG/qml/CMakeLists.txt` |
| **tests/CMakeLists.txt** | Test build config | `/home/treshen/Projects/QG/tests/CMakeLists.txt` |
| **packaging/debian/** | Native .deb packaging | `/home/treshen/Projects/QG/packaging/debian/` |

## Source Code Structure

```
src/
├── main.cpp                 # Entry point, OpenGL backend
├── app.cpp/hpp              # App bootstrap, DI wiring
├── config.cpp/hpp           # XDG paths, configuration
├── logging.cpp/hpp          # Structured logging
├── qml_plugin.cpp           # QML module registration
├── core/                    # Types, identity, events, errors, composition
├── filesystem/              # Enumerator, operations, trash, mounts, watcher, paths
├── media/                   # Formats, probe, thumbnails
├── sorting/                 # SortService
├── search/                  # SearchService, RecursiveSearchService
├── playback/                # PlaybackManager, Session, Context, MpvBackend, VideoSurface
├── workers/                 # TaskToken, PriorityPool, Tasks
├── persistence/             # Database, repositories, schema
├── models/                  # FolderModel, SearchProxy, ReelModel, PlacesModel, SelectionState
└── controllers/             # Browser, Search, Playback, Actions, Mounts, State, Thumb, Probe, Keyboard

qml/QG/
├── Main.qml                 # ApplicationWindow composition
├── Theme.qml                # Design tokens singleton
├── PathBar.qml              # Navigation, breadcrumbs, search, order, density
├── PlacesSidebar.qml        # Rail/panel sidebar
├── ContentView.qml          # GridView/ListView/Stage host
├── delegates/               # VideoCard, FolderCard, CompactRow, ManifestRow
├── playback/                # Stage, TransportBar, ReelPanel
├── strips/                  # ContextStrip, StatusStrip
├── overlays/                # StateOverlay, ToastHost
├── dialogs/                 # PropertiesDialog, RenameConflictDialog, ConfirmDeleteDialog
└── Icon.qml                 # Monoline SVG icon system
```

## Test Structure

```
tests/
├── unit/                    # 39 Qt Test binaries
├── integration/             # 8 integration test binaries
├── qml/                     # 8 Qt Quick Test files (2 failing)
├── performance/             # Benchmark infrastructure
└── benchmarks/              # Legacy performance tests
```

## Key Source Files by Subsystem

### Core
| File | Purpose |
|------|---------|
| `src/core/types.hpp` | Entry, FolderSnapshot, FolderDiff, SortSpec, EntryKind, ThumbnailState |
| `src/core/identity.cpp` | FileIdentity (Inode/Fingerprint/PathOnly) |
| `src/core/errors.hpp` | Result<T>, ErrorCode, DomainException hierarchy |
| `src/core/composition.cpp` | CoreServices wiring (single ownership root) |
| `src/core/events.hpp/cpp` | MpvIdleEvent, worker events |

### Filesystem
| File | Purpose |
|------|---------|
| `src/filesystem/enumerator.cpp` | Batched enumeration, TaskToken, filtering |
| `src/filesystem/operations.cpp` | Rename, move, copy, trash, reveal, open-with |
| `src/filesystem/trash.cpp` | gio trash + freedesktop fallback |
| `src/filesystem/mounts.cpp` | mountinfo parsing, GIO/UDisks2 hooks |
| `src/filesystem/watcher.cpp` | QFileSystemWatcher, 300ms debounce |
| `src/filesystem/paths.cpp` | XDG helpers, URI↔path |

### Media
| File | Purpose |
|------|---------|
| `src/media/formats.hpp` | Supported extensions (single source of truth) |
| `src/media/probe.cpp` | FFmpeg duration/resolution/rotation |
| `src/media/thumbnails.cpp` | Freedesktop cache + FFmpeg extraction |

### Playback
| File | Purpose |
|------|---------|
| `src/playback/mpv_backend.cpp` | Single mpv handle, LC_NUMERIC=C, property observation |
| `src/playback/video_surface.cpp` | QQuickItem + QSGRenderNode, mpv_render_context_render |
| `src/playback/playback_manager.cpp` | Singleton, deferred load, 5s tick, resume policy |
| `src/playback/playback_session.cpp` | Observable state (identity, path, duration, position, state) |
| `src/playback/playback_context.cpp` | Ordered videos from FolderModel, filter, prev/next |

### Models
| File | Purpose |
|------|---------|
| `src/models/folder_model.cpp` | Canonical QAbstractListModel, 14 roles, batch/diff |
| `src/models/search_proxy.cpp` | QSortFilterProxyModel + match positions |
| `src/models/reel_model.cpp` | Videos-only projection + playlist position |
| `src/models/places_model.cpp` | Sidebar groups: pinned/XDG/devices/recents |
| `src/models/selection_state.cpp` | Identities, focus index, range anchor |

### Controllers
| File | Purpose |
|------|---------|
| `src/controllers/browser_controller.cpp` | Navigation stack, enumeration, watcher, settings |
| `src/controllers/search_controller.cpp` | Query lifecycle, debounce, scope, proxy |
| `src/controllers/playback_controller.cpp` | PlaybackManager bridge, ordered context, Reel |
| `src/controllers/actions_controller.cpp` | File ops, toasts, undo, clipboard, menu items |
| `src/controllers/mounts_controller.cpp` | Volume discovery, eject, disconnect tracking |
| `src/controllers/state_controller.cpp` | Fav/Watched toggles, identity-keyed, DB rollback |
| `src/controllers/thumbnail_controller.cpp` | Validated delivery, visible-first scheduling |
| `src/controllers/probe_controller.cpp` | Coalesced resort hook (200ms) |
| `src/controllers/keyboard_framework.cpp` | Central shortcut router |
| `src/controllers/type_to_select_buffer.cpp` | 700ms timeout, printable key buffer |

## QML Components

| File | Purpose |
|------|---------|
| `qml/QG/Main.qml` | ApplicationWindow, 6 regions, keyboard map, dialogs |
| `qml/QG/Theme.qml` | Design tokens, HC bound to platform, RM app-toggle |
| `qml/QG/PathBar.qml` | Nav, breadcrumbs, search morph, order/density menus |
| `qml/QG/PlacesSidebar.qml` | Rail/panel, drag-to-pin, eject, drop target |
| `qml/QG/ContentView.qml` | Grid/List/Stage host, visibleRangeChanged |
| `qml/QG/delegates/VideoCard.qml` | Badges, playing ribbon, watched progress, hover actions |
| `qml/QG/delegates/FolderCard.qml` | 2×2 mosaic, count chip, hover enter, drop target |
| `qml/QG/delegates/CompactRow.qml` | 96×54 thumb, name, duration, size, date, badges |
| `qml/QG/delegates/ManifestRow.qml` | Columnar: thumb, name+path, duration, size, modified, state |
| `qml/QG/playback/Stage.qml` | Inline player, bloom, cinema fill, transport |
| `qml/QG/playback/TransportBar.qml` | Play/pause, prev/next, seek, volume, FS, close |
| `qml/QG/playback/ReelPanel.qml` | Ordered list, filter, footer prev/next/fav |
| `qml/QG/strips/ContextStrip.qml` | Folder name, counts, order, density selector |
| `qml/QG/strips/StatusStrip.qml` | Selection, free space, busy, transient text |
| `qml/QG/overlays/StateOverlay.qml` | Empty/denied/disconnected/error/no-results |
| `qml/QG/overlays/ToastHost.qml` | Bottom-center, 4s, Undo |
| `qml/QG/dialogs/*.qml` | Properties, RenameConflict, ConfirmDelete |
| `qml/QG/Icon.qml` | MultiEffect tinting, 16 SVGs |

## Scripts & Tools

| File | Purpose |
|------|---------|
| `scripts/generate_benchmark_dataset.py` | 50k/100k sparse file datasets |
| `tests/run_gl_test.sh` | Xvfb + Mesa llvmpipe GL tests, fractional scale |
| `hermes/qg-pipeline.py` | Autonomous development pipeline driver |

## Configuration Files

| File | Purpose |
|------|---------|
| `.hermes.md` | Project instructions for Hermes |
| `.gitignore` | Git ignore rules |
| `autobuilder.md` | Auto-builder configuration |

## Documentation Conventions

All architectural docs follow:
- **YAML frontmatter**: type, project, created, updated, status, tags, source
- **Mermaid diagrams** for visualizations
- **Tables** for structured comparisons
- **Source references** to actual files
- **Status badges**: ✅ Implemented, 🟡 Partial, ❌ Missing, 🟠 Incorrect, ⚪ Unverified