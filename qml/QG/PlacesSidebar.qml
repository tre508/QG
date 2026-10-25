import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects

import qg

// Places sidebar (DESIGN.md §3.3, §11.2, §14.1): rail (56px) ⇄ panel (240px),
// grouped Pinned / Places / Devices / Recent.
//
// Presentation only. Every row renders what the C++ PlacesModel publishes —
// icon name, label, tooltip, capabilities — so no filesystem classification is
// re-derived here. Activation, eject/unmount and pin intent are emitted as
// signals for the owning controller/service.
//
// Group headings are ordinary model rows (role `isHeader`), not ListView
// section delegates: every delegate then has the same height, so the view's
// content height is exact instead of estimated and the scroll range really does
// cover the last row (DECISIONS.md, FIX-UI-003).

Rectangle {
    id: sidebar

    // Any model exposing label/path/icon/kind/group/isHeader/connected/
    // removable/ejectable/canEject/canUnmount/mounted/volumeId/tooltip.
    property var model: null
    property bool expanded: true

    // High-DPI scaled dimensions
    readonly property int scaledRailWidth: Theme.scale(56)
    readonly property int scaledPanelWidth: Theme.scale(240)
    readonly property int scaledHeadingDivider: Theme.scale(8)
    readonly property int scaledRowHeight: Theme.scale(32)
    readonly property int scaledIconSize: Theme.scale(16)
    readonly property int scaledBadgeSize: Theme.scale(6)
    readonly property int scaledBadgeRadius: Theme.scale(3)
    readonly property int scaledActionSize: Theme.scale(22)
    readonly property int scaledActionIconSize: Theme.scale(14)

    readonly property int railWidth: scaledRailWidth
    readonly property int panelWidth: scaledPanelWidth

    objectName: "placesSidebar"
    implicitWidth: expanded ? panelWidth : railWidth
    color: Theme.surface1

    signal locationActivated(string path)
    signal volumeEjectRequested(string volumeId)
    signal volumeUnmountRequested(string volumeId)
    signal pinRequested(string path)
    signal unpinRequested(string path)
    signal fileDropped(string filePath, string targetDir, bool isMove)

    function activateLocation(path) {
        locationActivated(path)
    }

    // True when the model marks `row` as a group heading. Works with the C++
    // PlacesModel (isHeaderAt) and with a plain ListModel fixture (isHeader).
    function isHeaderIndex(i) {
        if (sidebar.model === null)
            return false
        if (sidebar.model.isHeaderAt !== undefined)
            return sidebar.model.isHeaderAt(i) === true
        if (sidebar.model.get !== undefined) {
            const item = sidebar.model.get(i)
            return item ? item.isHeader === true : false
        }
        return false
    }

    // Keyboard navigation skips group headings so the current row is always a
    // real location.
    function moveCurrent(step) {
        if (!list.model || list.count === 0)
            return
        let i = list.currentIndex < 0 ? (step > 0 ? -1 : list.count) : list.currentIndex
        let guard = 0
        do {
            i += step
            ++guard
        } while (i >= 0 && i < list.count && isHeaderIndex(i) && guard < list.count + 2)
        if (i >= 0 && i < list.count)
            list.currentIndex = i
    }

    function moveCurrentToBoundary(atEnd) {
        if (!list.model || list.count === 0)
            return
        let i = atEnd ? list.count - 1 : 0
        const step = atEnd ? -1 : 1
        let guard = 0
        while (i >= 0 && i < list.count && isHeaderIndex(i) && guard < list.count + 2) {
            i += step
            ++guard
        }
        if (i >= 0 && i < list.count)
            list.currentIndex = i
    }

    // Activates the keyboard-current row (Enter/Space). Headings have no path,
    // so activation is a no-op there.
    function activateCurrent() {
        const item = list.currentItem
        if (item && item.rowPath !== undefined && item.rowPath.length > 0) {
            sidebar.locationActivated(item.rowPath)
        }
    }

    ListView {
        id: list
        objectName: "placesList"
        anchors.fill: parent
        clip: true
        model: sidebar.model
        // Keyboard operability (DESIGN.md §13.3): reachable with Tab; the
        // explicit handlers below skip group headings. Focus-driven scrolling
        // is handled by the view itself.
        focus: false
        activeFocusOnTab: true
        keyNavigationEnabled: false
        // The device list grows and shrinks as mounts come and go; clamping to
        // the content bounds keeps the view from resting past the end.
        boundsBehavior: Flickable.StopAtBounds
        highlightMoveDuration: Theme.motionEnabled ? Theme.motionMicro : 0

        Keys.onUpPressed: sidebar.moveCurrent(-1)
        Keys.onDownPressed: sidebar.moveCurrent(1)
        Keys.onPressed: function (event) {
            if (event.key === Qt.Key_Home) {
                sidebar.moveCurrentToBoundary(false)
                event.accepted = true
            } else if (event.key === Qt.Key_End) {
                sidebar.moveCurrentToBoundary(true)
                event.accepted = true
            }
        }
        Keys.onReturnPressed: sidebar.activateCurrent()
        Keys.onEnterPressed: sidebar.activateCurrent()
        Keys.onSpacePressed: sidebar.activateCurrent()

        ScrollBar.vertical: ScrollBar {
            id: verticalScrollBar
            policy: ScrollBar.AsNeeded
        }

        delegate: Item {
            id: row
            objectName: "placeRow"
            // Uniform height for headings and locations: exact content height.
            // Never let a long label widen the sidebar (§11.2 truncation).
            width: list.width - (verticalScrollBar.visible ? verticalScrollBar.width : 0)
            height: scaledRowHeight

            readonly property string rowPath: model.path !== undefined ? model.path : ""
            readonly property bool rowIsHeader: model.isHeader === true
            readonly property bool rowDisconnected: model.connected === false
            readonly property bool rowCanEject: model.canEject === true
            readonly property bool rowCanUnmount: model.canUnmount === true
            readonly property bool rowHasAction: !rowIsHeader && (rowCanEject || rowCanUnmount)
            readonly property bool rowCurrent: !rowIsHeader && ListView.isCurrentItem && list.activeFocus
            readonly property bool rowActionVisible: rowHasAction
                                                    && (rowMouse.containsMouse || row.rowCurrent
                                                        || deviceAction.activeFocus)

            Accessible.role: row.rowIsHeader ? Accessible.StaticText : Accessible.ListItem
            Accessible.name: model.label !== undefined ? model.label : ""
            Accessible.description: model.tooltip !== undefined ? model.tooltip : ""
            Accessible.selected: row.rowCurrent

            // Hover / current-row background (never on a heading).
            Rectangle {
                anchors.fill: parent
                color: row.rowIsHeader ? "transparent"
                                       : (rowMouse.containsMouse ? Theme.surface3
                                                                 : (row.rowCurrent ? Theme.surface2
                                                                                   : "transparent"))
            }

            // Keyboard focus ring (never clipped, §2.9 / UI-13).
            Rectangle {
                objectName: "placeRowFocusRing"
                anchors.fill: parent
                anchors.margins: 1
                visible: row.rowCurrent
                color: "transparent"
                border.width: 1
                border.color: Theme.highContrast ? Theme.neonOutline : Theme.focus
                radius: Theme.radiusButton
            }

            // Group heading. In the rail the label is replaced by a divider.
            Text {
                objectName: "sectionHeaderLabel"
                anchors.left: parent.left
                anchors.leftMargin: Theme.space3
                anchors.verticalCenter: parent.verticalCenter
                visible: row.rowIsHeader && sidebar.expanded
                text: model.label !== undefined ? model.label : ""
                color: Theme.textLow
                font.pixelSize: Theme.fontMicro
                font.letterSpacing: 1
            }

            Rectangle {
                objectName: "sectionHeaderDivider"
                visible: row.rowIsHeader && !sidebar.expanded
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - Theme.space3 * 2
                height: 1
                color: Theme.hairline
            }

            Icon {
                id: rowIcon
                objectName: "placeRowIcon"
                anchors.left: parent.left
                anchors.leftMargin: Theme.space3
                anchors.verticalCenter: parent.verticalCenter
                width: scaledIconSize
                height: scaledIconSize
                visible: !row.rowIsHeader
                // Semantic name published by PlacesModel; no heuristics here.
                name: model.icon !== undefined ? model.icon : "folder"
                color: row.rowDisconnected ? Theme.textLow
                                           : (row.rowCurrent ? Theme.textHi : Theme.textMid)
            }

            Text {
                id: rowLabel
                objectName: "placeRowLabel"
                anchors.left: rowIcon.right
                anchors.leftMargin: Theme.space2
                anchors.right: actionSlot.left
                anchors.rightMargin: Theme.space1
                anchors.verticalCenter: parent.verticalCenter
                visible: sidebar.expanded && !row.rowIsHeader
                text: model.label !== undefined ? model.label : ""
                elide: Text.ElideRight
                color: row.rowDisconnected ? Theme.textLow
                                           : (row.rowCurrent ? Theme.textHi : Theme.textMid)
                font.pixelSize: Theme.fontBody
            }

            // Disconnected volume: amber badge (DESIGN.md §3.3), still clickable.
            Rectangle {
                id: disconnectedBadge
                objectName: "deviceDisconnectedBadge"
                visible: row.rowDisconnected && !row.rowIsHeader
                width: scaledBadgeSize
                height: scaledBadgeSize
                radius: scaledBadgeRadius
                color: Theme.warn
                // Anchored to the action slot — a sibling — rather than to the
                // button inside it. The slot reserves its width whether or not
                // the action is revealed, so the badge never jumps and never
                // collides with the eject/unmount affordance.
                anchors.right: actionSlot.visible ? actionSlot.left : parent.right
                anchors.rightMargin: Theme.space2
                anchors.verticalCenter: parent.verticalCenter
            }

            // Capability-driven device action slot (§17.4): an eject affordance
            // only where the platform reports eject, otherwise an unmount
            // affordance where it reports unmount — never a dead button.
            Item {
                id: actionSlot
                objectName: "deviceActionSlot"
                // Above the row click area (VIS-006: the device action was
                // previously unreachable because the MouseArea covered it).
                z: 1
                anchors.right: parent.right
                anchors.rightMargin: Theme.space1
                anchors.verticalCenter: parent.verticalCenter
                width: scaledActionSize
                height: scaledActionSize
                visible: sidebar.expanded && !row.rowIsHeader

                ToolButton {
                    id: deviceAction
                    objectName: "deviceActionButton"
                    anchors.fill: parent
                    visible: row.rowActionVisible
                    activeFocusOnTab: true
                    Accessible.name: model.canEject === true
                        ? qsTr("Eject %1").arg(model.label)
                        : qsTr("Unmount %1").arg(model.label)

                    contentItem: Icon {
                        objectName: "deviceActionIcon"
                        anchors.centerIn: parent
                        name: model.canEject === true ? "eject" : "unmount"
                        color: deviceAction.hovered || deviceAction.activeFocus ? Theme.textHi
                                                                               : Theme.textMid
                        size: scaledActionIconSize
                    }

                    background: Rectangle {
                        color: "transparent"
                        border.width: deviceAction.activeFocus ? 1 : 0
                        border.color: Theme.highContrast ? Theme.neonOutline : Theme.focus
                        radius: Theme.radiusChip
                    }

                    onClicked: {
                        if (model.canEject === true)
                            sidebar.volumeEjectRequested(model.volumeId)
                        else
                            sidebar.volumeUnmountRequested(model.volumeId)
                    }

                    ToolTip.visible: hovered
                    ToolTip.text: model.canEject === true
                        ? qsTr("Eject %1").arg(model.label)
                        : qsTr("Unmount %1").arg(model.label)
                }
            }

            // Row tooltip: shows the target path, or the device status when the
            // label is elided or hidden in rail mode.
            ToolTip.visible: !row.rowIsHeader && rowMouse.containsMouse && !row.rowActionVisible
            ToolTip.delay: 600
            ToolTip.text: model.tooltip !== undefined && model.tooltip.length > 0
                          ? model.tooltip : model.label

            // Right-click: pin or unpin this location. Unpinning removes only
            // the bookmark — the folder itself is never touched.
            Menu {
                id: rowMenu
                objectName: "placeRowMenu"
                MenuItem {
                    objectName: "placeRowPinToggle"
                    text: model.group === 0 ? qsTr("Remove from Sidebar") : qsTr("Pin to Sidebar")
                    onTriggered: {
                        if (model.group === 0)
                            sidebar.unpinRequested(row.rowPath)
                        else
                            sidebar.pinRequested(row.rowPath)
                    }
                }
            }

            // Row click target. Declared before the action button and kept below
            // it (z) so the device action always receives its own clicks.
            // Headings are not clickable.
            MouseArea {
                id: rowMouse
                objectName: "placeRowClickArea"
                anchors.fill: parent
                enabled: !row.rowIsHeader
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                z: 0
                onClicked: function (mouse) {
                    if (mouse.button === Qt.RightButton)
                        rowMenu.popup()
                    else
                        sidebar.activateLocation(row.rowPath)
                }
            }
        }
    }

    // Drag a folder onto the sidebar to pin it; drag a file onto a sidebar place
    // to navigate + copy/move (REM-014 / D-27).
    DropArea {
        id: dropArea
        anchors.fill: parent
        keys: ["text/uri-list"]
        onDropped: {
            if (drop.hasUrls && drop.urls.length > 0) {
                const urlStr = drop.urls[0].toString()
                const urlPath = urlStr.indexOf("file://") === 0 ? urlStr.substring(7) : urlStr
                // If it's a folder path being pinned (existing behavior)
                if (drop.mimeData && drop.mimeData["qg/folder-pin"]) {
                    sidebar.pinRequested(urlPath)
                    return
                }
                // File drop onto sidebar place: navigate to folder and handle drop
                if (sidebar.model && sidebar.model.count !== undefined) {
                    for (let i = 0; i < sidebar.model.count; ++i) {
                        const item = sidebar.model.get ? sidebar.model.get(i) : null
                        if (!item) continue
                        const p = (item.path !== undefined) ? item.path : ""
                        if (p !== "" && (urlPath === p || urlPath.startsWith(p + "/"))) {
                            sidebar.locationActivated(p)
                            sidebar.fileDropped(urlPath, p, !(drop.modifiers & Qt.ControlModifier))
                            return
                        }
                    }
                }
                // If no matching sidebar place, treat as pin attempt
                sidebar.pinRequested(urlPath)
            }
        }
    }
}
