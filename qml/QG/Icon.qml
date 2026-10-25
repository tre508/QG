// Icon (DESIGN.md §2.3, §2.4, §2.9): shared monoline SVG renderer.
//
// Renders a named monoline icon from the qg module resource at the requested
// size and tints it with a single `color` (one role at a time). Tinting uses
// QtQuick.Effects MultiEffect colorization, which recolors the white line-art
// monochrome source to any Theme token at runtime (a plain Image cannot be
// recolored). High-contrast (§2.9) is honored automatically: the caller passes
// a Theme token, whose value already swaps to its saturated equivalent when
// Theme.highContrast is true, so no per-icon duplicate is needed.
//
// Unknown names fall back to a neutral dot instead of crashing.

import QtQuick
import QtQuick.Effects

Item {
    id: icon

    property string name: ""
    property int size: 20
    property color color: "white"

    // name → resource filename (single registry; authoring an SVG is one entry
    // here plus the file under resources/icons/monoline). An empty value means
    // "unknown" and renders the fallback.
    readonly property string fileName: ({
        "play": "play.svg",
        "pause": "pause.svg",
        "prev": "prev.svg",
        "next": "next.svg",
        "seek-back": "seek-back.svg",
        "seek-forward": "seek-forward.svg",
        "volume": "volume.svg",
        "mute": "mute.svg",
        "fullscreen": "fullscreen.svg",
        "close": "close.svg",
        "warn": "warn.svg",
        "favorite": "favorite.svg",
        "watched": "watched.svg",
        "folder": "folder.svg",
        "trash": "trash.svg",
        "search": "search.svg",
        "lock": "lock.svg",
        "more": "more.svg",
        "placeholder": "placeholder.svg",
        "chevron-right": "chevron-right.svg",
        "parent": "parent.svg",
        "refresh": "refresh.svg",
        "sort": "sort.svg",
        "density": "density.svg",
        "device": "device.svg",
        "eject": "eject.svg",
        "edit": "edit.svg",
        "cut": "cut.svg",
        "copy": "copy.svg",
        "reveal": "reveal.svg",
        "open-with": "open-with.svg",
        "properties": "properties.svg",
        "delete-permanent": "delete-permanent.svg",
        "activity": "activity.svg",
        "density-gallery": "density-gallery.svg",
        "density-standard": "density-standard.svg",
        "density-compact": "density-compact.svg",
        // Sidebar semantics (FIX-UI-003): places, pinned/recent, device kinds
        // and the unmount affordance. Registered here AND shipped in
        // qml/CMakeLists.txt — an unregistered SVG renders as a fallback dot.
        "home": "home.svg",
        "videos": "videos.svg",
        "downloads": "downloads.svg",
        "desktop": "desktop.svg",
        "documents": "documents.svg",
        "recent": "recent.svg",
        "pin": "pin.svg",
        "drive": "drive.svg",
        "drive-removable": "drive-removable.svg",
        "network": "network.svg",
        "unmount": "unmount.svg"
    })[icon.name] || ""

    readonly property string source: icon.fileName.length > 0
        ? "qrc:/qt/qml/qg/icons/" + icon.fileName : ""

    implicitWidth: icon.size
    implicitHeight: icon.size
    width: icon.size
    height: icon.size

    // Load status of the underlying image, exposed for tests.
    readonly property int status: img.status

    // Line-art source (white on transparent), tinted by MultiEffect below.
    Image {
        id: img
        anchors.fill: parent
        visible: icon.source.length > 0
        sourceSize: Qt.size(icon.size, icon.size)
        source: icon.source
        fillMode: Image.PreserveAspectFit
        smooth: true
    }

    // Color from a single role (§2.4): full colorization replaces the white
    // stroke with `icon.color`.
    MultiEffect {
        anchors.fill: img
        source: img
        visible: icon.source.length > 0 && img.status === Image.Ready
        colorization: 1.0
        colorizationColor: icon.color
    }

    // Fallback: a neutral dot when the name is not in the registry.
    Rectangle {
        objectName: "iconFallback"
        anchors.centerIn: parent
        width: Math.max(4, icon.size * 0.3)
        height: width
        radius: width / 2
        color: icon.color
        visible: icon.source.length === 0
    }
}
