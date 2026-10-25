---
type: conventions
project: QG
created: 2026-10-20
updated: 2026-10-20
status: verified
tags:
  - conventions
  - cpp
  - implementation
source:
  - src/
  - DESIGN.md
  - ARCHITECTURE_CURRENT.md
---

# C++ Conventions

## Core Principles

1. **Domain Logic in C++** — All filesystem, persistence, playback, search logic in C++
2. **Result-based Errors** — `Result<T>` / `Result<void>` with `Error{code,message,path,systemError}`
3. **TaskToken Cancellation** — Every async operation checks token at boundaries
3. **Single Ownership** — `CoreServices` constructs/wires all non-UI subsystems once
4. **Qt Boundaries** — No exceptions across Qt signal/QML boundaries
5. **Thread Safety** — Models mutated only on GUI thread via queued signals

## File Organization

```
src/
├── main.cpp                    # Entry point, OpenGL backend
├── app.cpp/hpp                 # App bootstrap, DI wiring
├── config.cpp/hpp              # XDG paths, configuration
├── logging.cpp/hpp             # Structured logging
├── qml_plugin.cpp              # QML module registration
├── core/                       # Types, identity, events, errors, composition
├── filesystem/                 # Enumerator, operations, trash, mounts, watcher, paths
├── media/                      # Formats, probe, thumbnails
├── sorting/                    # SortService
├── search/                     # SearchService, RecursiveSearchService
├── playback/                   # PlaybackManager, Session, Context, MpvBackend, VideoSurface
├── workers/                    # TaskToken, PriorityPool, Tasks
├── persistence/                # Database, repositories, schema
├── models/                     # FolderModel, SearchProxy, ReelModel, PlacesModel, SelectionState
└── controllers/                # Browser, Search, Playback, Actions, Mounts, State, Thumb, Probe, Keyboard
```

## Naming Conventions

| Element | Convention | Example |
|---------|------------|---------|
| Classes | PascalCase | `BrowserController`, `FolderModel` |
| Methods | camelCase | `navigate()`, `applySnapshot()` |
| Member variables | `m_` prefix | `m_currentPath`, `m_token` |
| Static constants | `k` prefix + PascalCase | `kMaxBatchSize` |
| Enums | PascalCase | `SortKey`, `ViewDensity` |
| Enum values | PascalCase | `SortKey::Name`, `ViewDensity::Standard` |
| Namespaces | lowercase | `qg` |
| Header guards | `QG_<PATH>_<FILE>_HPP` | `QG_CONTROLLERS_BROWSER_CONTROLLER_HPP` |

## Error Handling

```cpp
// Result type (all services)
template<typename T>
class Result {
    std::optional<T> value_;
    std::optional<Error> error_;
public:
    static Result success(T&& v) { return Result(std::move(v)); }
    static Result failure(Error&& e) { return Result(std::move(e)); }
    bool has_value() const { return value_.has_value(); }
    T& value() { return *value_; }
    const Error& error() const { return *error_; }
};

// Error structure
struct Error {
    ErrorCode code;
    QString message;
    QString path;
    int systemError = 0;
    
    static Error make(ErrorCode c, const QString& msg, const QString& p = {}, int sys = 0);
};

// Domain exceptions (NEVER cross Qt signal/QML boundaries)
class DomainException : public std::exception { ... };
class AccessDenied : public DomainException { ... };
class VolumeUnavailable : public DomainException { ... };
class NameCollision : public DomainException { ... };
class UnsupportedMedia : public DomainException { ... };
class OperationFailed : public DomainException { ... };

// Controller maps errors to overlays/toasts
void BrowserController::onError(const Error& err) {
    if (err.code == ErrorCode::AccessDenied) {
        emit accessDenied(path);
    } else {
        emit errorOccurred(path, err.message);
    }
}
```

## TaskToken & Cancellation

```cpp
// TaskToken (src/workers/task_token.hpp)
class TaskToken {
    std::uint64_t generation_;      // Monotonic counter
    std::string path_or_identity_;  // Work scope
    std::atomic<bool> cancelled_;   // Cooperative cancellation
public:
    std::uint64_t generation() const { return generation_; }
    bool isCancelled() const { return cancelled_.load(); }
    void cancel() { cancelled_.store(true); }
};

// Worker submission (src/workers/tasks.hpp)
template<typename Work, typename Deliver>
void submitTask(QThreadPool* pool, TaskPriority priority,
                std::shared_ptr<TaskToken> token, QObject* context,
                Work&& work, Deliver&& deliver);

// Controller usage
void BrowserController::startEnumeration(bool reset) {
    m_token = std::make_shared<TaskToken>();  // New generation
    const auto token = m_token;
    
    submitTask(WorkerPools::io(), TaskPriority::Interactive, token, this,
        [path, token, showHidden, result]() {
            *result = enumerator.enumerate(path, *token, options);
        },
        [this, token, result, reset]() {
            if (token != m_token || token->isCancelled()) return; // Stale!
            if (reset) m_model->applySnapshot(*result);
            else m_model->applyDiff(computeFolderDiff(...));
            m_model->reorder(m_sortSpec);
        });
}
```

## Thread Safety Rules

### GUI Thread Only
- All QML-accessible objects (controllers, models)
- Model mutations (`applySnapshot`, `applyDiff`, `dataChanged`, `layoutChanged`)
- QML property bindings
- Qt signal/slot connections (direct or queued)

### Worker Threads (I/O Pool, CPU/Media Pool)
- Filesystem enumeration (`readdir`, `fstatat`)
- File operations (copy, move, trash)
- Thumbnail extraction (FFmpeg decode/scale)
- Media probing (FFmpeg format open)
- Mount info parsing (`/proc/self/mountinfo`)

### Cross-Thread Communication
```cpp
// Worker → GUI: Queued signal or invokeMethod
QMetaObject::invokeMethod(controller, [this, result]() {
    controller->onResult(result);
}, Qt::QueuedConnection);

// Or signal with queued connection
emit workerFinished(result);  // Connected with Qt::QueuedConnection

// GUI → Worker: Submit new task with new TaskToken
// Cancellation: Advance generation (new TaskToken) — old tasks self-cancel
```

### libmpv Threads
- **mpv internal**: Decode, filter, audio — owned by libmpv
- **wakeupCallback**: Posts `MpvIdleEvent` to Qt thread
- **Render thread**: `VideoRenderNode` calls `mpv_render_context_render()`
- **Qt thread**: Property observation via `mpv_observe_property` → signals

## RAII Patterns

```cpp
// SQLite
class Statement {
    sqlite3_stmt* m_stmt = nullptr;
    ~Statement() { if (m_stmt) sqlite3_finalize(m_stmt); }
};

// File descriptors
struct FdDeleter { void operator()(int* fd) { if (fd && *fd >= 0) close(*fd); delete fd; } };
using UniqueFd = std::unique_ptr<int, FdDeleter>;

// mpv handle
~MpvBackend() { shutdown(); }
void shutdown() {
    if (renderCtx_) mpv_render_context_free(renderCtx_);
    if (handle_) mpv_terminate_destroy(handle_);
}

// Timers
QTimer timer;
timer.setSingleShot(true);
connect(&timer, &QTimer::timeout, this, &Class::onTimeout);
```

## Signal/Slot Conventions

```cpp
// Controller → Model (GUI thread, direct or queued)
connect(browser, &BrowserController::currentPathChanged,
        thumbnailController, [this]() { thumbnailController->beginGeneration(); });

// Model → QML (GUI thread, direct)
emit dataChanged(index, index, {ThumbnailRole, ThumbnailStateRole});
emit layoutChanged();

// Service → Controller (queued from worker)
connect(service, &Service::finished,
        controller, &Controller::onServiceFinished,
        Qt::QueuedConnection);

// libmpv → Qt thread
mpv_set_wakeup_callback(handle_, [](void* u) {
    QCoreApplication::postEvent(static_cast<MpvBackend*>(u), new MpvIdleEvent());
});
```

## Controller Patterns

```cpp
class BrowserController : public QObject {
    Q_OBJECT
    
    // Injected dependencies
    FolderModel* m_model;
    DirectoryWatcher* m_watcher;
    SettingsRepository* m_settings;
    
    // Owned state
    QString m_currentPath;
    QList<QString> m_backStack, m_forwardStack;
    std::shared_ptr<TaskToken> m_token;
    
public:
    explicit BrowserController(QObject* parent = nullptr);
    
    // Navigation (entry points from QML)
    Q_INVOKABLE bool navigate(const QString& path);
    Q_INVOKABLE void back();
    Q_INVOKABLE void forward();
    Q_INVOKABLE void parent();
    Q_INVOKABLE void refresh();
    
    // Settings
    Q_INVOKABLE void setSort(int key, bool descending, bool foldersFirst);
    Q_INVOKABLE void setDensity(int density);
    
signals:
    void currentPathChanged(const QString& path);
    void historyChanged();
    void busyChanged(bool busy);
    void freeSpaceChanged();
    void errorOccurred(const QString& path, const QString& message);
    void parentRestored(const QString& childName);
};
```

## Service Patterns

```cpp
class FileOperationsService : public QObject {
    Q_OBJECT
    
public:
    // Synchronous (called from worker thread)
    OperationResult renameSync(const QString& path, const QString& newName);
    OperationResult moveSync(const QStringList& paths, const QString& targetDir);
    OperationResult copySync(const QStringList& paths, const QString& targetDir);
    OperationResult trashSync(const QStringList& paths);
    OperationResult deletePermanentlySync(const QStringList& paths);
    void revealSync(const QString& path);
    void openWithSync(const QString& path, const QString& appId);
    
signals:
    // Async completion (emitted from worker thread → queued to controller)
    void operationFinished(const QString& kind, const OperationResult& result);
};
```

## Model Patterns

```cpp
class FolderModel : public QAbstractListModel {
    Q_OBJECT
    
    enum Roles {
        IdentityRole = Qt::UserRole + 1,
        PathRole, NameRole, KindRole, IsFolderRole, IsVideoRole,
        ThumbnailRole, ThumbnailStateRole,
        DurationRole, SizeRole, MtimeRole,
        FavoriteRole, WatchedRole, ProgressRole,
        WidthRole, HeightRole
    };
    
    QList<Entry> m_entries;
    QHash<QString, QVariant> m_thumbnails;
    QHash<QString, ThumbnailState> m_thumbnailStates;
    
public:
    // Snapshot (full replace)
    void applySnapshot(const FolderSnapshot& snapshot);
    
    // Batch append (incremental enumeration)
    void applyBatch(const FolderSnapshot& batch);
    
    // Diff (external change)
    void applyDiff(const FolderDiff& diff);
    
    // Targeted updates (thumbnail, probe, state)
    void updateThumbnail(const QString& identity, const QVariant& image, ThumbnailState state);
    void updateProbe(const QString& identity, std::optional<qint64> duration, 
                     std::optional<int> width, std::optional<int> height);
    void updateState(const QString& identity, std::optional<bool> favorite,
                     std::optional<bool> watched, std::optional<double> progress);
    
    // Identity-keyed access
    int indexOfIdentity(const QString& identity) const;
    QString identityAt(int row) const;
    QList<Entry> entriesMatching(std::function<bool(const Entry&)> predicate) const;
    
    // Sort
    void reorder(const SortSpec& spec);  // Emits layoutChanged()
};
```

## QML Registration

```cpp
// src/core/composition.cpp
void registerCoreQmlTypes() {
    // Non-creatable: QG owns single instances
    qmlRegisterUncreatableType<BrowserController>("qg.core", 1, 0, "BrowserController", "QG owns");
    qmlRegisterUncreatableType<FolderModel>("qg.core", 1, 0, "FolderModel", "QG owns");
    qmlRegisterUncreatableType<SearchProxy>("qg.core", 1, 0, "SearchProxy", "QG owns");
    // ... all controllers/models
    
    // Creatable: Stage creates VideoSurfaceItem
    qmlRegisterType<VideoSurfaceItem>("qg.core", 1, 0, "VideoSurfaceItem");
}
```

## Testing Patterns

```cpp
// Qt Test
class TestBrowserController : public QObject {
    Q_OBJECT
private slots:
    void navigate_validPath_populatesModel();
    void back_forward_restoresStack();
    void rapidNavigation_cancelsStaleResults();
    
    void initTestCase() { /* setup */ }
    void cleanupTestCase() { /* teardown */ }
};
QTEST_MAIN(TestBrowserController)
#include "test_browser_controller.moc"
```

## Build Integration

```cmake
# tests/unit/CMakeLists.txt
add_executable(test_browser_controller test_browser_controller.cpp)
target_link_libraries(test_browser_controller PRIVATE qg_core Qt6::Test)
add_test(NAME qg_browser_controller COMMAND test_browser_controller)

# Test names must contain keyword for ctest -R
set_tests_properties(qg_browser_controller PROPERTIES LABELS "unit")
```

## Anti-Patterns to Avoid

| ❌ Don't | ✅ Do |
|----------|-------|
| Throw exceptions across Qt boundaries | Return `Result<T>` |
| Mutate models from worker threads | Use queued signals to GUI thread |
| Block GUI thread on I/O | Submit to worker pool with TaskToken |
| Poll mpv for state | Observe properties via `mpv_observe_property` |
| Create second mpv handle | Use `PlaybackManager` singleton |
| Reparent VideoSurface for Cinema | Use layout state (`cinema` property) |
| Hardcode pixel values | Use `Theme.scale()` in QML |
| Versioned QML imports | Unversioned: `import QtQuick` |
| Raw Unicode glyphs | `Icon { name: "..." }` |