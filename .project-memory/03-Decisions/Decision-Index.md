---
type: decisions
project: QG
created: 2026-10-20
updated: 2026-10-20
status: verified
tags:
  - decisions
  - index
  - architecture
source:
  - DECISIONS.md
---

# QG Decision Index

## Architectural Decisions

| ID | Title | Status | Summary |
|----|-------|--------|---------|
| DEC-001 | Native C++20 | ✅ Implemented | C++20, warnings enabled, no exceptions across Qt boundaries |
| DEC-002 | Qt 6 Quick/QML Presentation | ✅ Implemented | QML for all UI; C++ for domain logic |
| DEC-003 | C++ Ownership of Domain Logic | ✅ Implemented | Services/controllers/models in C++; QML presentation-only |
| DEC-004 | Ubuntu/Linux Desktop Target | ✅ Implemented | XDG paths, GIO, UDisks2, freedesktop standards |
| DEC-005 | X11/xcb Primary Runtime | ✅ Implemented | Wayland-compatible path retained |
| DEC-006 | OpenGL Qt Quick Backend | ✅ Implemented | Forced before first QQuickWindow |
| DEC-007 | libmpv C API, Single Owner | ✅ Implemented | MpvBackend sole owner; lazy creation |
| DEC-008 | One Active Playback Session | ✅ Implemented | PlaybackManager singleton; single handle |
| DEC-009 | mpv_render_context + Qt Scene Graph | ✅ Implemented | VideoSurfaceItem (QQuickItem + QSGRenderNode) |
| DEC-010 | In-window Cinema | ✅ Implemented | Layout state only; no new window; no surface reparenting |
| DEC-011 | Filesystem as Source of Truth | ✅ Implemented | No library ingestion; identity-based state |
| DEC-012 | SQLite Metadata Persistence (WAL) | ✅ Implemented | Per-thread connections; migrations |
| DEC-013 | Non-blocking GUI Architecture | ✅ Implemented | Bounded pools + TaskToken + stale-result rejection |
| DEC-014 | Authoritative Ordered Folder/Playback Context | ✅ Implemented | FolderModel drives Browse, Reel, PlaybackContext |
| DEC-015 | CMake + Ninja Build | ✅ Implemented | qt_add_qml_module for QML compilation |
| DEC-016 | Native .deb Primary Packaging | 🟡 Partial | Metadata exists; build/install unvalidated |
| DEC-017 | QML Module Registration + Static Plugin | ✅ Implemented | qg_core module; qml_plugin.cpp |
| DEC-018 | Single Unified Entry Model Row | ✅ Implemented | FolderModel::Entry for folders + videos |
| DEC-019 | Result-based Error Handling | ✅ Implemented | Result<T>/Result<void> with Error{code,message,path,systemError} |
| DEC-020 | Media Identity Kinds | ✅ Implemented | Inode / Fingerprint / PathOnly with encoding |
| DEC-021 | qg_core Static Non-UI Library | ✅ Implemented | CoreServices + all non-UI subsystems |
| DEC-022 | Single-source SQLite Schema | ✅ Implemented | schema.sql → schema_sql.hpp (CMake) |
| DEC-023 | SQLite Dependency Provisioning | ✅ Implemented | system libsqlite3-dev (3.46.1) |
| DEC-024 | Enumerator Batching, Filtering, Error Policy | ✅ Implemented | ~200 entries/batch; filtered at source |
| DEC-025 | FolderModel Roles and Projection Hook | ✅ Implemented | 14 roles + entriesMatching() |
| DEC-026 | BrowserController Enumerates on I/O Pool | ✅ Implemented | Applies on GUI thread via queued signals |
| DEC-027 | Identity-based Diff for External Change | ✅ Implemented | QFileSystemWatcher + 300ms debounce |
| DEC-028 | Kernel-first Mount Discovery with Enricher/Ejector Hooks | ✅ Implemented | /proc/self/mountinfo + optional GIO/UDisks2 |
| DEC-029 | Injected Controller/Model for Navigation QML | ✅ Implemented | App injects browserController, folderModel, etc. |
| DEC-030 | StatusStrip Contract + Real Free-space Source | ✅ Implemented | statvfs via BrowserController |
| DEC-031 | Thumbnail Cache Key + Freedesktop-first Lookup | ✅ Implemented | identity + mtime_ns + size + version |
| DEC-032 | In-process FFmpeg Extraction on CPU Pool | ✅ Implemented | libavformat/libavcodec/libswscale |
| DEC-033 | Visible-first Scheduling + Bounded LRU | ✅ Implemented | Priority queue; demote on scroll away |
| DEC-034 | Failure Markers + Bounded Retry Policy | ✅ Implemented | 2 retries then terminal failure |
| DEC-035 | Validated Thumbnail Application to Model | ✅ Implemented | Token + mtime/size validation |
| DEC-036 | In-process Media Probing, Cached and Bounded | ✅ Implemented | ≤2 concurrent; Background priority |
| DEC-037 | Identity↔Path Mapping and Reconciliation | ✅ Implemented | PathRepository rename/reconcile |
| DEC-038 | Identity-keyed StateController | ✅ Implemented | Favorite/Watched/Progress with DB rollback |
| DEC-039 | Per-folder View Settings Owned by BrowserController | ✅ Implemented | sort:<path>, density:<path> |
| DEC-040 | ProbeController Validated Delivery + Coalesced Resort | ✅ Implemented | 200ms coalesced resort hook |
| DEC-041 | SortService Deterministic Total Order | ✅ Implemented | Folders-first; locale-aware; unknown durations last |
| DEC-042 | FolderModel::reorder Single Sort Application Point | ✅ Implemented | Emits layoutChanged |
| DEC-043 | CoreServices Single Ownership Root | ✅ Implemented | Constructs/wires all non-UI subsystems once |
| DEC-044 | Grid Columns Derive From Density Target; Leftover Becomes Centred Gutters | ✅ Implemented | Largest `n` whose cards reach the §3.5 target, clamped to `[min_card, max_card]`; single `leftMargin`, `rightMargin` 0. See [[standard-grid-seven-columns]] |
| DEC-045 | Logic-first Implementation with Deferred Batch Testing (process) | ✅ Active (policy) | Implement + static review + document per fix; builds/tests/launches/screenshots deferred to an authorized batch phase. Status vocabulary `IMPLEMENTED`/`STATICALLY REVIEWED`/`TESTING DEFERRED`/`VERIFIED`; backlog in `DEFERRED_TESTS.md` → [[Deferred-Test-Register]]; workflow contract in [[Agent-Workflow]] |

## Runtime Fixes (Not in DECISIONS.md but Critical)

| Fix | Description | Location |
|-----|-------------|----------|
| LC_NUMERIC=C | mpv_create() returns NULL unless C locale | `MpvBackend::createHandle()` |
| Deferred loadfile | Must wait for render context ready | `PlaybackManager::openWithIdentity()` |
| Observed state only | Transport never polls mpv | `MpvBackend` property observation |
| Ordered context from model | Reel is projection of FolderModel | `PlaybackContext::build()` |
| Resume via injected loader | Position read from DB by identity | `PlaybackManager::setPositionLoader()` |
| Card intents routed once | Row index → Main.qml → controller | `Main.qml` dispatch functions |
| Controller owns truth | QML owns surface only | `ActionsController` operationFinished |

## Pending Decisions

| Area | Question | Status |
|------|----------|--------|
| Collections UI | Sidebar group vs modal picker? | 🟡 Planned |
| Recursive search UI | Scope chip vs separate panel? | 🟡 Planned |
| Wayland testing | Priority vs X11? | ⚪ Unverified |
| High-DPI strategy | Fractional scale rounding? | ⚪ Unverified |
| Performance targets | 300ms first screenful achievable? | 🟡 Benchmark needed |