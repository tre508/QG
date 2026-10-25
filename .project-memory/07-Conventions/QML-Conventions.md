---
type: conventions
project: QG
created: 2026-10-20
updated: 2026-10-20
status: verified
tags:
  - conventions
  - qml
  - ui
source:
  - qml/QG/
  - DESIGN.md §2-§12
---

# QML Conventions

## Core Principles

1. **Presentation Only** — No business logic, no filesystem access, no persistence, no playback control
2. **Unversioned Imports** — `import QtQuick` NOT `import QtQuick 2.15`
3. **Theme Tokens** — All colors, spacing, fonts, scaling via `Theme.*`
4. **Icon System** — `Icon { name: "..." }` for all icons; no Unicode glyphs
5. **Scale Awareness** — `Theme.scale(px)` for all hardcoded pixel values

## Import Rules

```qml
// GOOD
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import qg

// BAD — versioned imports break type resolution under Qt 6
import QtQuick 2.15
import QtQuick.Controls 2.15
```

## Component Template

```qml
// ComponentName.qml
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import qg

Item {
    id: root
    
    // 1. Properties (typed, default null for injected C++)
    property var controller: null
    property var model: null
    property bool enabled: true
    
    // 2. Signals (QML → C++)
    signal actionRequested(string action, var payload)
    
    // 3. Connections (C++ → QML)
    Connections {
        target: controller
        function onStateChanged(newState) {
            // Update visual state
        }
    }
    
    // 4. Layout & Visuals
    // ...
}
```

## Theme Token Reference

### Colors
```qml
// Surfaces (deep → shallow)
Theme.surface0  // Deepest background
Theme.surface1  // Cards, panels
Theme.surface2  // Hover states
Theme.surface3  // Active/selected
Theme.surface4  // Highest emphasis

// Text
Theme.textHi    // Primary text
Theme.textMed   // Secondary text
Theme.textLow   // Muted/disabled
Theme.textMuted // Placeholder

// Neons (state indicators)
Theme.signal    // Primary action, focus
Theme.favorite  // Favorite/heart
Theme.watched   // Watched/check
Theme.danger    // Destructive (trash, delete)
Theme.warn      // Warning (disconnected)
Theme.play      // Play button

// High Contrast
Theme.highContrast          // Boolean: Qt.styleHints.accessibility.contrastPreference
Theme.neonOutline           // White outline for HC focus rings
Theme.motionEnabled         // Readonly: !reducedMotion
```

### Geometry
```qml
// Spacing
Theme.space1  // 4px
Theme.space2  // 8px
Theme.space3  // 12px
Theme.space4  // 24px

// Radius
Theme.radius1 // 4px
Theme.radius2 // 8px
Theme.radius3 // 12px
Theme.radius4 // 16px

// Fonts
Theme.fontMicro  // 10px
Theme.fontMeta   // 11px
Theme.fontCaption// 12px
Theme.fontBody   // 14px
Theme.fontSubtitle // 16px
Theme.fontTitle  // 28px

// Scaling (ALWAYS use for hardcoded pixels)
Theme.scale(200)  // Returns 200 * devicePixelRatio (snapped to 1.0/1.25/1.5/2.0)
```

## Icon System

```qml
// GOOD
Icon {
    name: "favorite"
    color: Theme.favorite
    width: Theme.scale(20)
    height: Theme.scale(20)
}

// BAD — raw Unicode glyph
Text { text: "♥"; color: "#ff0066" }

// Available icons (16 shipped + 17 custom in resources/icons/monoline/)
// Core: play, favorite, watched, lock, placeholder, warn, chevron-right
// Files: folder, device, eject, edit, cut, copy, reveal, open-with, properties, delete-permanent
// UI: activity, more, search, refresh, chevron-left, chevron-right
// Density: density-gallery, density-standard, density-compact
```

## Drag & Drop

```qml
// Drag Source (VideoCard, FolderCard, CompactRow)
MouseArea {
    id: dragArea
    anchors.fill: parent
    drag.target: parent
    Drag.active: dragArea.drag.active
    Drag.mimeData: { "text/uri-list": path.length > 0 ? ["file://" + path] : [] }
    Drag.dragType: Drag.Automatic
    drag.threshold: 10
}

// Drop Target (ContentView, FolderCard, PlacesSidebar)
DropArea {
    anchors.fill: parent
    keys: ["text/uri-list"]
    onEntered: { /* visual feedback */ }
    onDropped: {
        const urls = drop.urls.filter(u => u.startsWith("file://"))
        const paths = urls.map(u => u.slice(7))  // strip "file://"
        // Validate, emit dropRequested(paths, targetDir, isMove)
    }
}
```

## Responsive Layouts

```qml
// GridView columns (Gallery/Standard)
property int columnCount: Math.max(1, Math.floor(width / (Theme.scale(220) + Theme.space3)))

// ListView (Compact/Manifest) — single column, full width

// Responsive delegates
Loader {
    sourceComponent: density === ContentView.Gallery ? galleryDelegate :
                     density === ContentView.Standard ? standardDelegate :
                     compactDelegate
}
```

## Animation & Motion

```qml
// Respect reduced motion
Behavior on x {
    enabled: Theme.motionEnabled
    NumberAnimation { duration: 220; easing.type: Easing.OutCubic }
}

// Instant when reduced motion
PropertyAnimation {
    duration: Theme.motionEnabled ? 220 : 0
}
```

## Focus & Accessibility

```qml
// Focus ring (uses neonOutline in HC mode)
Rectangle {
    anchors.fill: parent
    anchors.margins: -2
    border.width: 2
    border.color: Theme.highContrast ? Theme.neonOutline : Theme.signal
    visible: root.activeFocus
    radius: Theme.radius2
}

// Screen reader names
Accessible.name: "Video: " + name
Accessible.role: Accessible.Button
```

## Keyboard Shortcuts

```qml
// Window-scoped shortcuts in Main.qml only
Shortcut {
    sequence: "Ctrl+F"
    enabled: !window.cinema && !window.textEditing
    onActivated: window.routeKey("view.search")
}

// Text-input suppression (uniform)
enabled: !window.textEditing
```

## Delegates

```qml
// VideoCard.qml pattern
Item {
    width: Theme.scale(320)
    height: density === ContentView.Gallery ? Theme.scale(400) : Theme.scale(240)
    
    // Badge zones (fixed positions)
    // Top-left: favorite
    // Top-right: watched
    // Bottom-left: progress (watched)
    // Bottom-right: duration
    // Center: play overlay (when playing)
    // Hover: quick actions (play, favorite, overflow)
    
    // Inline rename
    TextField {
        // Extension locked by default
        // Live validation
        // Enter → renameCommitted
    }
}

// FolderCard.qml
Item {
    // 2×2 mosaic thumbnail
    // Count chip: "12v · 3"
    // Hover: ▸ enter affordance
    // Drop target
}
```

## State Overlays

```qml
// ContentView handles via contentState precedence:
// disconnected > accessDenied > error > loading > empty
StateOverlay {
    visible: contentState.length > 0 && !sessionActive
    property string appliedState: contentState
    onAppliedStateChanged: applyState(appliedState, "")
    onActionTriggered: contentStateAction()
}
```

## Dialogs

```qml
// PropertiesDialog — real fs stats + QG metadata
// RenameConflictDialog — Keep Both / Overwrite / Cancel
// ConfirmDeleteDialog — count + first name
// ToastHost — bottom-center, 4s auto-dismiss, Undo affordance
```

## Common Patterns

### Conditional Rendering
```qml
// Cinema hides chrome
Rectangle {
    visible: !window.cinema
    implicitHeight: window.cinema ? 0 : 48
}
```

### Controller Calls
```qml
// Always null-check injected controllers
function handleAction() {
    if (controller && controller.methodName) {
        controller.methodName(args)
    }
}
```

### Model Access
```qml
// Via injected model (FolderModel or SearchProxy)
model.pathAt(index)
model.identityAt(index)
model.favoriteAt(index)
// etc.
```

## Anti-Patterns to Avoid

| ❌ Don't | ✅ Do |
|----------|-------|
| `import QtQuick 2.15` | `import QtQuick` |
| `color: "#1a1a2e"` | `color: Theme.surface1` |
| `Text { text: "♥" }` | `Icon { name: "favorite" }` |
| `width: 200` | `width: Theme.scale(200)` |
| `font.pixelSize: 14` | `font.pixelSize: Theme.fontBody` |
| `border.color: "red"` | `border.color: Theme.danger` |
| `QFileDialog` in QML | `ActionsController.openWith()` |
| `mpv` property in QML | `PlaybackSession` bindings |
| `setTimeout`/`Timer` for polling | mpv property observation |
| Business logic in QML | Controller methods |