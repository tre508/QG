---
type: conventions
project: QG
created: 2026-10-20
updated: 2026-10-20
status: verified
tags:
  - conventions
  - coding-standards
  - cpp
  - qml
source:
  - DESIGN.md
  - ARCHITECTURE_CURRENT.md
  - src/
  - qml/
---

# QG Coding Standards

## C++ Standards

### Language & Build
- **Standard**: C++20
- **Warnings**: Enabled (`-Wall -Wextra -Wpedantic`)
- **Exceptions**: Not used across Qt signal/QML boundaries
- **Build**: CMake + Ninja, `qt_add_qml_module` for QML compilation

### Architecture Rules
1. **Layering**: qml → controllers → models → services → workers → persistence → playback
2. **Ownership**: C++ owns all domain logic; QML is presentation-only
3. **Filesystem authority**: Real paths only; no library ingestion
4. **Single mpv handle**: `MpvBackend` sole owner; lazy creation
5. **Non-blocking GUI**: All I/O/media on bounded worker pools with TaskToken

### Naming Conventions

| Element | Convention | Example |
|---------|------------|---------|
| Classes | PascalCase | `BrowserController`, `FolderModel` |
| Methods | camelCase | `navigate()`, `applySnapshot()` |
| Member variables | `m_` prefix | `m_currentPath`, `m_token` |
| Constants | UPPER_SNAKE_CASE | `MAX_BATCH_SIZE` |
| Enums | PascalCase | `SortKey`, `ViewDensity` |
| Enum values | PascalCase | `SortKey::Name`, `ViewDensity::Standard` |
| Namespaces | lowercase | `qg` |
| Files | snake_case | `browser_controller.cpp`, `folder_model.hpp` |

### Code Patterns

#### Result-based Error Handling
```cpp
// Services return Result<T> or Result<void>
Result<void> FileOperationsService::rename(const QString& path, const QString& newName);

// Error structure
struct Error {
    ErrorCode code;
    QString message;
    QString path;
    int systemError;
};

// Domain exceptions (never cross Qt/QML boundaries)
class AccessDenied : public DomainException {};
class VolumeUnavailable : public DomainException {};
```

#### TaskToken Cancellation
```cpp
// Always check token at chunk boundaries
while (hasMoreWork && !token->isCancelled()) {
    doWork();
}

// Stale result rejection in controllers
if (token != m_token || token->isCancelled()) {
    return; // Stale: newer navigation superseded this
}
```

#### Qt Signal/Slot Connections
```cpp
// Queued connections for cross-thread delivery
QObject::connect(worker, &Worker::finished,
                 controller, &Controller::onResult,
                 Qt::QueuedConnection);

// Lambda with context capture
QObject::connect(browser, &BrowserController::currentPathChanged,
                 thumbnailController, [this]() { thumbnailController->beginGeneration(); });
```

#### RAII Resource Management
```cpp
// File descriptors, SQLite statements, mpv resources, timers, temp files
std::unique_ptr<Resource, Deleter> resource;
// or QScopedPointer / QPointer for Qt objects
```

### C++ File Organization
```
src/
├── <subsystem>/
│   ├── <component>.hpp      # Interface
│   ├── <component>.cpp      # Implementation
│   └── <component>_test.cpp # Unit tests (in tests/unit/)
```

## QML Standards

### Language & Imports
- **Unversioned imports only**: `import QtQuick` NOT `import QtQuick 2.15`
- **Qt Quick Controls 2**: Used for dialogs, menus
- **No business logic in QML**: All domain logic in C++ controllers/services

### Component Structure
```qml
// File: ComponentName.qml
import QtQuick
import QtQuick.Controls
import qg  // Internal module

Item {
    id: root
    
    // Properties (typed where possible)
    property var controller: null
    property bool enabled: true
    
    // Signals (for QML → C++ communication)
    signal actionTriggered(string action, var payload)
    
    // Connections to C++ controllers
    Connections {
        target: controller
        function onStateChanged() { /* update QML */ }
    }
    
    // Child items
    // ...
}
```

### Naming Conventions

| Element | Convention | Example |
|---------|------------|---------|
| Components | PascalCase | `VideoCard.qml`, `PathBar.qml` |
| Properties | camelCase | `currentPath`, `searchActive` |
| Signals | camelCase with `on` prefix | `onActivatedItem`, `onSearchQueryChanged` |
| Methods | camelCase | `navigate()`, `refresh()` |
| IDs | camelCase | `contentView`, `pathBar` |
| Object names | kebab-case | `objectName: "pathBarRegion"` |

### QML Patterns

#### Controller Injection
```qml
// Main.qml injects controllers after load
property var browserController: null
property var folderModel: null

// Usage in delegates
contentView.pathForIndex(index)  // Via injected model
browserController.navigate(path)  // Via injected controller
```

#### Theme Token Usage
```qml
import QtQuick
import qg  // Theme singleton

Rectangle {
    color: Theme.surface1
    border.color: Theme.neonFocus  // Not raw hex!
    width: Theme.scale(200)        // Scale for High-DPI
}
```

#### Icon System
```qml
// NEVER use raw Unicode glyphs
// BAD: text: "♥"
// GOOD:
Icon {
    name: "favorite"
    color: Theme.favorite
    width: Theme.scale(20)
    height: Theme.scale(20)
}
```

#### Drag & Drop
```qml
MouseArea {
    drag.target: parent
    Drag.active: dragArea.drag.active
    Drag.mimeData: { "text/uri-list": ["file://" + path] }
    Drag.dragType: Drag.Automatic
    drag.threshold: 10
}

DropArea {
    keys: ["text/uri-list"]
    onEntered: { /* visual feedback */ }
    onDropped: { handleDrop(drop.urls) }
}
```

#### Responsive Layouts
```qml
// Use Theme.scale() for all hardcoded pixels
// Use Theme.fontBody, Theme.fontMeta for font sizes
// Use Theme.space1-4 for spacing
// Responsive columns via width / (itemWidth + spacing)
```

### QML File Organization
```
qml/QG/
├── Main.qml                 # ApplicationWindow
├── Theme.qml                # Design tokens singleton
├── Icon.qml                 # SVG registry + MultiEffect tinting
├── PathBar.qml              # Navigation
├── PlacesSidebar.qml        # Sidebar
├── ContentView.qml          # Grid/List/Stage host
├── delegates/               # VideoCard, FolderCard, etc.
├── playback/                # Stage, TransportBar, ReelPanel
├── strips/                  # ContextStrip, StatusStrip
├── overlays/                # StateOverlay, ToastHost
├── dialogs/                 # Properties, RenameConflict, ConfirmDelete
└── Format.qml               # Formatting helpers
```

## Design System Conventions

### Color Tokens (Theme.qml)
- **Surfaces**: `surface0` (deepest) → `surface4` (highest)
- **Text**: `textHi`, `textMed`, `textLow`, `textMuted`
- **Neons**: `signal` (primary), `favorite`, `watched`, `danger`, `warn`, `play`
- **High Contrast**: Bound to `Qt.styleHints.accessibility.contrastPreference`
  - Surfaces widen separation
  - Text brightens
  - Neons saturate
  - `neonOutline` token for focus rings

### Geometry Tokens
- **Spacing**: `space1` (4px) → `space4` (24px)
- **Radius**: `radius1` (4px) → `radius4` (16px)
- **Fonts**: `fontMicro` (10px) → `fontTitle` (28px)
- **Scaling**: `Theme.scale(px)` for all hardcoded pixels

### Reduced Motion
- **App-toggle authoritative**: `Theme.motionEnabled` (readonly = `!reducedMotion`)
- **Qt 6.10**: No `reducedAnimation` signal — platform clause recorded as unavailable
- **Drives**: Instant bloom when disabled

## Git Conventions

### Commit Messages
```
<type>(<scope>): <subject>

<body>

<footer>
```
Types: `feat`, `fix`, `refactor`, `perf`, `test`, `docs`, `chore`

### Branching
- `main` — Stable releases
- Feature branches: `feature/<name>`
- Fix branches: `fix/<issue>`

## Review Checklist

Before submitting changes:
- [ ] Follows layering (no QML business logic, no C++ in QML)
- [ ] Uses Theme tokens (no raw hex colors)
- [ ] Uses Icon component (no Unicode glyphs)
- [ ] Uses `Theme.scale()` for hardcoded pixels
- [ ] Result-based error handling in services
- [ ] TaskToken cancellation in workers
- [ ] Queued connections for cross-thread
- [ ] No model mutations from workers
- [ ] No direct filesystem/persistence/playback in QML
- [ ] Unversioned QML imports
- [ ] Tests updated for new functionality