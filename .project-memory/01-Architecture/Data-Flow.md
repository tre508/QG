---
type: architecture
project: QG
created: 2026-10-20
updated: 2026-10-20
status: verified
tags:
  - architecture
  - data-flow
  - state-management
source:
  - DESIGN.md
  - ARCHITECTURE_CURRENT.md
  - ARCHITECTURE_OVERVIEW.md
---

# QG Data Flow & State Management

## Core Data Flows

### 1. Folder Navigation

```
User Action (sidebar/breadcrumb/Ctrl+L)
    │
    ▼
PathBar → navigate(path) → BrowserController
    │
    ├─► Push navigation stack (back/forward)
    ├─► Advance TaskToken generation (cancels in-flight work)
    ├─► Update currentPath, emit currentPathChanged
    ├─► Update free space (statvfs)
    ├─► Load per-folder settings (sort, density)
    ├─► DirectoryWatcher.rearm(new path)
    │
    ▼
FilesystemEnumerator.enumerate(path, token) on I/O Pool
    │
    ├─► Batched FolderSnapshot (~200 entries) via queued signal
    │     (filtered at source: dirs + video extensions only)
    │
    ▼
BrowserController.onSnapshot(token, snapshot)
    │
    ├─► Validate token (drop if stale)
    ├─► FolderModel.applySnapshot() or applyDiff()
    │     └─► beginResetModel/endResetModel or minimal rowsInserted/Removed/Changed
    ├─► model.reorder(currentSortSpec)
    ├─► ThumbnailController.requestVisible(first, last)
    ├─► ProbeController.requestVisible(first, last)
    ├─► PersistenceService.recordRecent(path)
    │
    ▼
QML View Updates (dataChanged/layoutChanged)
    │
    ├─► GridView/ListView: skeleton → cached thumbs → live content
    ├─► Focus: first item (or restored on parent())
    └─► Selection: cleared
```

### 2. Search (Manifest)

```
Ctrl+F → PathBar.searchQueryChanged(query)
    │
    ▼
SearchController.setQuery(query) [debounced ~80ms]
    │
    ├─► SearchProxy.setFilterFixedString(query)
    │     └─► filterAcceptsRow: casefold substring + word-boundary boost
    │
    ▼
ContentView.enterManifest(proxy, query)
    │
    ├─► Swaps to Manifest ListView (columnar)
    ├─► Matched substrings highlighted in signal blue
    ├─► Shows containing path column
    │
    ▼
Esc → SearchController.clear()
    │
    ▼
ContentView.exitManifest() → Browse restored with scroll/selection intact
```

### 3. Recursive Search (Architecture Ready, UI Not Wired)

```
SearchController.setScope(true) [Subfolders scope]
    │
    ▼
RecursiveSearchService.start(query, rootDir, config) on I/O Pool
    │
    ├─► Iterative walk (breadth-first, maxDepth configurable)
    ├─► SearchService.match per directory
    ├─► RecursiveSearchResult: filename, containingPath, isFolder, sizeBytes, mtimeNs, identity
    ├─► Batched via batchReady(batch, generation) queued signal
    │
    ▼
RecursiveSearchModel receives batches
    │
    ├─► 11 roles for ManifestRow compatibility
    ├─► Identity-keyed for state persistence
    │
    ▼
ContentView.recursiveModel = RecursiveSearchModel
ContentView.searchRecursive = true
```

### 4. Playback (Stage → Cinema)

```
Double-click/Enter on video → ContentView.onActivatedItem(index, path, isFolder=false)
    │
    ▼
PlaybackController.play(path, identity)
    │
    ├─► FolderModel.entriesMatching(video) → ordered list (active sort)
    ├─► Build PlaybackContext (videos only, filter: All/Fav/Unwatched)
    ├─► Find currentIndex by identity
    │
    ▼
PlaybackManager.openWithIdentity(path, identity, resumeStart)
    │
    ├─► Flush outgoing session position (5s tick + pause/stop/close)
    ├─► Ensure single mpv handle (lazy create, LC_NUMERIC=C)
    ├─► Ensure render context (VideoSurfaceItem → mpv_render_context_create)
    ├─► If no render context: hold pending load
    ├─► mpv_command("loadfile", path, "start=" + resumeStart)
    ├─► tickTimer.start(5s) → positionTick(identity, seconds)
    │
    ▼
MpvBackend property observation (pause, duration, time-pos, seekable, eof-reached, idle-active, volume, mute)
    │
    ▼
PlaybackManager → PlaybackSession (observable state)
    │
    ▼
Stage bloom animation (220ms) → grid reflows → ReelPanel opens at currentIndex
    │
    ▼
Cinema (F11/F/Double-click): Main.qml.cinema = true
    │
    ├─► Hide PathBar, ContextStrip, Sidebar, Reel, StatusStrip
    ├─► Stage fills window (same session, no reload)
    ├─► Cursor auto-hide after 2s idle
    │
    ▼
Esc → exitCinema() → Stage → Browse (scroll/selection preserved)
```

### 5. Thumbnail Pipeline

```
Enumeration → Entry with identity
    │
    ▼
Cache Key = identity + mtime_ns + size + thumbnailer_version
    │
    ├─► Freedesktop cache lookup (Thumb::URI, Thumb::MTime)
    │     └─► Hit → Model update (ThumbnailRole, ThumbnailStateRole=Ok)
    │
    ▼
ThumbnailService.extractThumbnail() on CPU/Media Pool
    │
    ├─► Priority: Visible (Interactive) → Adjacent (Normal) → Background (Background)
    ├─► FFmpeg: avformat_open_input → avcodec_find_decoder → sws_scale → QImage
    ├─► Write to $XDG_CACHE_HOME/qg/thumbnails/ + DB row (thumbnails table)
    ├─► Validate: token current? entry exists? source mtime/size unchanged?
    │
    ▼
ThumbnailController.validatedDelivery()
    │
    ▼
FolderModel.updateThumbnail(identity, QImage, ThumbnailState::Ok)
    │
    ▼
dataChanged(index, index, {ThumbnailRole, ThumbnailStateRole})
    │
    ▼
QML delegate updates thumbnail
```

### 6. External Change Detection

```
Filesystem event (create/delete/rename/modify)
    │
    ▼
QFileSystemWatcher.directoryChanged(path) [300ms debounce]
    │
    ▼
BrowserController.refresh() → startEnumeration(false)
    │
    ▼
FilesystemEnumerator.enumerate(currentPath, currentToken)
    │
    ▼
FolderSnapshot → BrowserController.onSnapshot()
    │
    ▼
FolderDiff = computeFolderDiff(model.entries(), snapshot.entries)
    │     └─► Keyed by identity (st_dev, st_ino)
    │
    ▼
FolderModel.applyDiff(diff)
    │     ├─► Removed: beginRemoveRows/endRemoveRows
    │     ├─► Changed: dataChanged(PathRole, NameRole, SizeRole, MtimeRole, ThumbnailRole, ThumbnailStateRole)
    │     └─► Added: beginInsertRows/endInsertRows
    │
    ▼
Thumbnail/Probe schedulers: requestVisible(first, last) for new entries
```

### 7. Position Persistence

```
PlaybackManager.tickTimer (5s while playing)
    │
    ▼
PlaybackManager.positionTick(identity, seconds)
    │
    ▼
StateController.recordPosition(identity, ms) [throttled]
    │
    ▼
MediaStateRepository.positionMs(identity, ms) [SQLite WAL]
    │
    ▼
On resume: PlaybackManager.resumeStartFor(identity) → injected loader → MediaStateRepository.positionMs()
    │
    ▼
mpv loadfile with start=resumePosition
```

## State Management

### Identity System

```cpp
// core/identity.hpp
enum class IdentityKind { Inode, Fingerprint, PathOnly };

struct FileIdentity {
    IdentityKind kind;
    std::string key;  // "inode:<dev>:<ino>" | "fp:<hash>" | "path:<path>"
};
```

- **Primary**: `(st_dev, st_ino)` — stable across rename/move within filesystem
- **Fallback**: Fingerprint `(size, mtime_ns, first 64 KiB hash)` — network/odd filesystems
- **Degradation**: Path-only with explicit disclosure in Properties

### Persistent State (SQLite)

| Table | Key | Contents |
|-------|-----|----------|
| `paths` | `path PK` | Path ↔ identity mapping (rename-safe) |
| `media_state` | `identity PK` | Favorite, watched, watch_count, last_position_ms, duration, resolution, poster_ms |
| `thumbnails` | `identity PK` | Cache path, source mtime/size, thumb_ms, state, fail_count |
| `recent_locations` | `path PK` | Capped recent locations |
| `bookmarks` | `id PK` | Pinned folders (path, label, position) |
| `settings` | `key PK` | Per-folder sort (`sort:<path>`), density (`density:<path>`), resume policy |
| `collections` / `collection_items` | (future) | User-authored playlists |

### Transient State (In-Memory)

| Component | State |
|-----------|-------|
| `BrowserController` | Navigation stack, currentPath, sortSpec, density, busy, freeSpaceBytes |
| `SearchController` | Query, scope, proxy, result summary |
| `PlaybackController` | cinemaActive, reelFilter, reelVisible, current session/context |
| `ActionsController` | clipboardPaths, clipboardCut, lastTrashPath, lastTrashOriginalPath |
| `MountsController` | disconnected, disconnectedDrive, disconnectedMount |
| `FolderModel` | entries[], thumbnails{}, thumbnailStates{}, videoCount, folderCount |
| `SelectionState` | selected[], focusIndex, anchorIndex |

## State Precedence (Main.qml)

```
contentState = disconnected > accessDenied > error > loading > empty > ""
```

- `disconnected`: Inside a volume that was physically disconnected
- `accessDenied`: Permission denied on current directory
- `error`: Enumeration error (non-permission)
- `loading`: Busy AND count == 0
- `empty`: Not busy AND count == 0 (but NOT folder-only dirs)