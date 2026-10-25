---
type: reference
project: QG
created: 2026-10-20
updated: 2026-10-20
status: verified
tags:
  - reference
  - glossary
  - terminology
source:
  - DESIGN.md
  - ARCHITECTURE_CURRENT.md
---

# QG Glossary

## A

**ActionsController** — C++ controller orchestrating file operations (rename, trash, move, copy, reveal, open-with, properties), clipboard, toasts, and undo. Owns internal clipboard state.

**applyDiff** — FolderModel method applying minimal mutations (rowsRemoved/rowsInserted/dataChanged) from a FolderDiff computed by identity.

**applySnapshot** — FolderModel method replacing entire model contents via beginResetModel/endResetModel.

## B

**Background Pool** — CPU/Media worker pool (min(cpu, 6) threads) for thumbnail extraction, media probing, scaling. Priority: Background.

**Bloom** — Stage entry animation: video card expands inline to become player container; grid reflows around it.

**Bookmarks** — Pinned folders persisted in SQLite (`bookmarks` table); shown in PlacesSidebar Pinned group.

## C

**Cinema** — Fullscreen playback mode: MainWindow chrome hides, Stage fills window, same mpv session continues (no reload, no rebuffer). Entered via F11, F, or double-click on Stage.

**ContentState** — Main.qml computed property determining overlay: `disconnected > accessDenied > error > loading > empty`. Folder-only directories never show empty.

**ContentView** — QML component hosting three view modes: Browse (GridView), Compact/Manifest (ListView), and Stage. Manages scroll, focus, type-to-select, visibleRangeChanged.

**CoreServices** — Single ownership root (DEC-043) constructing and wiring all non-UI subsystems: StateController, ThumbnailController, ProbeController, SearchProxy, SearchController, FileOperationsService, TrashService, ActionsController, MountsController, ReelModel, SelectionState, TypeToSelectBuffer, MpvBackend, PlaybackManager, PlaybackContext, PlaybackController, KeyboardFramework.

## D

**DEC-XXX** — Architectural decision identifiers (e.g., DEC-007: single mpv handle). Documented in DECISIONS.md.

**Density** — View mode per folder: Gallery (large cards), Standard (medium), Compact (rows), Manifest (search results). Remembered per folder in settings.

**DirectoryWatcher** — QFileSystemWatcher wrapper with 300ms debounce; emits identity-based diff for external changes.

## E

**Entry** — Single model row (folder or video) in FolderModel. Contains: identity, path, name, kind, thumbnail, duration, size, mtime, favorite, watched, progress, width, height.

**EnumeratorTask** — Worker task running FilesystemEnumerator on I/O pool with TaskToken; publishes FolderSnapshot batches via queued signal.

## F

**FileIdentity** — Stable file key: Primary = (st_dev, st_ino); Fallback = fingerprint (size, mtime_ns, first 64 KiB hash); Degradation = path-only with disclosure. Encoded as `inode:<dev>:<ino>`, `fp:<hash>`, `path:<path>`.

**FilesystemEnumerator** — Service scanning directory on I/O pool: batched (~200 entries), filtered at source (dirs + video extensions), per-entry errors, TaskToken cancellation.

**FolderDiff** — Identity-keyed diff between two folder snapshots: removedIdentities, changed (updated entries), added (new entries).

**FolderModel** — Canonical QAbstractListModel for a directory (folders + videos). 14 roles. Powers Browse, Reel (via projection), PlaybackContext. applySnapshot/applyBatch/applyDiff.

**FolderSnapshot** — Batched enumeration result: entries[], complete flag, error list.

## G

**Generation** — Monotonic uint64 counter incremented on navigation/search/sort change; used by TaskToken to detect stale async results.

**GIO** — GLib I/O library; used for trash (`gio trash`), Open With (`GAppInfo`), mount enrichment (`GVolumeMonitor`).

## H

**High Contrast** — Bound to `Qt.styleHints.accessibility.contrastPreference`; tokens widen separations, brighten text, saturate neons. `neonOutline` token for focus rings.

## I

**I/O Pool** — Worker pool (min(4, cpu) threads) for enumeration, file ops, mount discovery. Priorities: Interactive, Normal, Background.

**Identity** — See FileIdentity.

**Icon.qml** — QML component rendering monoline SVGs with MultiEffect tinting via Theme tokens. 16 shipped + 17 custom SVGs.

## L

**LC_NUMERIC=C** — Required locale fix: `mpv_create()` returns NULL unless C numeric locale. Set in MpvBackend::createHandle().

## M

**Manifest** — Search intensity: PathBar morphs to search field, ContentView shows columnar ListView with matched substring highlighting.

**MediaProbeService** — In-process FFmpeg (libavformat/libavcodec) for duration/resolution/rotation/tracks. Cached by identity+mtime/size. ≤2 concurrent, Background priority.

**MountService** — Parses `/proc/self/mountinfo` (5s poll), filters pseudo-filesystems, optional GIO/UDisks2 enrichment, emits volumesChanged.

**MpvBackend** — SOLE owner of mpv handle. Lazy creation with LC_NUMERIC=C. mpv_render_context for OpenGL. Property observation (pause, duration, time-pos, seekable, eof-reached, idle-active, volume, mute). wakeupCallback → MpvIdleEvent → Qt thread.

## N

**Navigation Stack** — BrowserController maintains backStack/forwardStack for Back/Forward/Parent operations. Parent() saves child name for focus restoration.

**neonOutline** — Theme token (white) for high-contrast focus rings and neon button borders.

## P

**PathBar** — QML component: back/forward/parent/refresh buttons, breadcrumbs↔path entry morph, search field, order/density/overflow menus. Emits onScopeToggled.

**PlacesModel** — Sidebar data: Pinned (bookmarks), Places (XDG dirs), Devices (mounts), Recents. Updated by MountsController and RecentRepository.

**PlacesSidebar** — QML rail (56px) / panel (240px) with drag-to-pin, device eject button, drop target.

**PlaybackContext** — Ordered video list derived from FolderModel (active sort + Reel filter). Identity-keyed currentIndex. prev/next stop at ends.

**PlaybackController** — Bridges UI intent → PlaybackManager. Owns ordered playback context, Reel wiring, Stage/Cinema layout state.

**PlaybackManager** — Process singleton. Single mpv handle. Deferred loadfile until renderContextReady. 5s position tick + flush on pause/stop/close/EOF. Resume policy (Always/Ask/Never) via injected loader.

**PlaybackSession** — Observable state: identity, path, duration, position, state (Idle/Loading/Playing/Paused/Ended/Error), volume (0..1), muted, seekable, errorMessage.

**PriorityPool** — Bounded QThreadPool with 3 priority levels (Interactive, Normal, Background) via reserved thread counts.

## Q

**QSGRenderNode** — Qt Scene Graph render node; VideoRenderNode calls `mpv_render_context_render()` on render thread.

## R

**RecursiveSearchService** — Architecture extension point: iterative breadth-first walk on I/O pool, SearchService matching per directory, batched RecursiveSearchResult with containingPath, sizeBytes, mtimeNs, identity. Not wired in MVP UI.

**ReelModel** — Videos-only projection of FolderModel + playlist position (currentIndex). Filter: All/Favorites/Unwatched.

**ReelPanel** — QML sidebar showing ordered folder videos, current highlight, filter, footer prev/next/fav.

**Render Thread** — Qt Quick Scene Graph thread; VideoRenderNode executes `mpv_render_context_render()` here.

**Result<T>** — Typed result wrapper: success(value) or failure(Error{code,message,path,systemError}). Used by all services.

## S

**SearchController** — Query lifecycle (debounce ~80ms), scope (current-dir vs recursive), proxy wiring, result summary.

**SearchProxy** — QSortFilterProxyModel over FolderModel for current query. Exposes match ranges for Manifest highlighting.

**SelectionState** — Tracks selected identities, focus index, range anchor. Three independent concepts: Focus (cyan), Selection (signal), Playback (mint).

**SortService** — Deterministic total order: folders-first, locale-aware name, unknown durations last. Tie-break by name then path.

**SortSpec** — Sort key (Name/Modified/Duration/Size/Watched/Favorite), direction (Asc/Desc), foldersFirst flag.

**Stage** — Inline player container: VideoSurface + TransportBar + scrim. Bloom/collapse geometry. Cinema = full window.

**StateController** — Favorite/Watched toggles (identity-keyed). Writes through MediaStateRepository, emits model-role updates, DB rollback on failure.

**StateOverlay** — QML overlay for empty/access-denied/disconnected/error/no-results with recovery actions.

**TaskToken** — Cancellation token: generation + path/identity + atomic cancelled flag. Checked at chunk boundaries; stale results dropped.

**ThumbnailController** — Validated delivery: priority queue (visible→adjacent→background), LRU cache, failure markers + bounded retry (2 then terminal).

**ThumbnailService** — Freedesktop cache first (Thumb::URI/Thumb::MTime), then FFmpeg extraction (libavformat/libavcodec/libswscale). Cache key = identity + mtime_ns + size + version.

**TransportBar** — Playback controls: play/pause, prev/next, ±10s, seek, volume, fullscreen, close. Auto-hide after 2.5s pointer inactivity.

**TypeToSelectBuffer** — 700ms printable key buffer for type-to-select in ContentView.

## U

**UDisks2** — D-Bus service for removable volume notifications, eject. Used via QtDBus in MountService enricher/ejector hooks.

## V

**VideoSurfaceItem** — QQuickItem + QSGRenderNode. Only place mpv frames enter scene graph. renderContextReady() handshake.

**VideoRenderNode** — QSGRenderNode subclass; `render()` calls `mpv_render_context_render()` into Qt FBO.

## W

**Wakeup Callback** — `mpv_set_wakeup_callback()` posts MpvIdleEvent to Qt thread for property/event processing.

**Worker Pools** — Two bounded QThreadPools: I/O (min(4,cpu)) + CPU/Media (min(cpu,6)). Task submission via `submitTask()`.

## X

**XDG Base Directories** — QG directories under XDG: data ($XDG_DATA_HOME/qg), cache ($XDG_CACHE_HOME/qg), state ($XDG_STATE_HOME/qg), config ($XDG_CONFIG_HOME/qg).