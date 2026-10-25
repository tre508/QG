import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects

import qg

// Path Bar (DESIGN.md §3.2, §4, §23.2): navigation controls, clickable
// breadcrumbs that morph into a path entry (Ctrl+L), and the right-edge
// action cluster (search, order, density, overflow). Presentation only —
// every action is forwarded to the injected `controller` (a
// BrowserController-shaped object).
//
// FIX-UI-002: restored the four-button navigation group (back/forward/parent/
// refresh) with themed monoline icons + tooltips + accessible names + focus
// overlays; fixed the doubled root slash in the breadcrumb (separator is now
// "›", rendered only between segments); implemented the Density and Overflow
// buttons/menus that were empty placeholder slots.

Rectangle {
    id: pathBar

    // BrowserController-shaped object: currentPath/canGoBack/canGoForward/
    // density/showHidden properties and back/forward/parent/refresh/navigate/
    // setDensityValue/setShowHidden functions.
    property var controller: null

    readonly property string currentPath: controller ? controller.currentPath : ""
    readonly property bool editing: entryField.activeFocus && entryField.visible
    property bool entryMode: false
    property string entryError: ""

    // Current sort state (from the controller; defaults when not injected).
    readonly property int sortKey: controller && controller.sortKey !== undefined
                                   ? controller.sortKey : 0
    readonly property bool sortDescending: controller && controller.sortDescending !== undefined
                                           ? controller.sortDescending : false
    readonly property bool foldersFirst: controller && controller.foldersFirst !== undefined
                                         ? controller.foldersFirst : true

    // Current view density (ViewDensity enum: Gallery=0, Standard=1, Compact=2).
    readonly property int density: controller && controller.density !== undefined
                                  ? controller.density : 1

    // VIS-013: the Density button must *show* the active mode instead of a
    // generic grid. Icon and label derive from the same `density` value that the
    // menu checks, so the button cannot disagree with per-folder state.
    readonly property string densityIconName: density === 0 ? "density-gallery"
                                            : density === 2 ? "density-compact"
                                                            : "density-standard"
    readonly property string densityLabel: density === 0 ? qsTr("Gallery")
                                         : density === 2 ? qsTr("Compact")
                                                         : qsTr("Standard")
    // Show-hidden toggle (DESIGN.md §23.2: hidden files off by default).
    readonly property bool showHidden: controller && controller.showHidden !== undefined
                                       ? controller.showHidden : false

    // Emitted when the user chooses a different sort/direction/folders-first.
    signal orderChanged(int sortKey, bool sortDescending, bool foldersFirst)
    // Emitted when the user picks a density mode.
    signal densitySelected(int density)
    // Emitted when the user toggles "Show hidden files".
    signal showHiddenToggled(bool show)

    // Search morph (Prompt 42): Ctrl+F toggles inline search in the track area.
    property bool searchActive: false
    property int searchResultCount: 0
    property bool searchSubfolders: false
    signal searchQueryChanged(string query)

    function toggleSearch() {
        searchActive = !searchActive
        searchMorphOverlay.visible = searchActive
        if (searchActive) {
            searchField.text = ""
            searchField.forceActiveFocus()
        }
    }

    function exitSearch() {
        searchActive = false
        searchMorphOverlay.visible = false
        searchQueryChanged("")
    }

    // Search scope toggle signal (Prompt 42 / FIX-TEST-001)
    signal scopeToggled(bool subfolders)

    // Single forwarding point for all menu actions (also the TestCase contract).
    function selectOrder(key, descending, foldersFirst) {
        orderChanged(key, descending, foldersFirst)
    }

    // Single forwarding point for density selection (testable without opening
    // the popup — MenuItem.onTriggered is not invocable from a Quick test).
    function selectDensity(d) {
        densitySelected(d)
    }

    // Single forwarding point for the show-hidden toggle.
    function toggleShowHidden() {
        showHiddenToggled(!showHidden)
    }

    // Ctrl+O is routed centrally (REMEDIATION-012 / D-11) and lands here.
    function openOrderMenu() {
        orderMenu.popup()
    }

    // High-DPI scaled dimensions
    readonly property int scaledPathBarHeight: Theme.scale(48)
    readonly property int scaledChipHeight: Theme.scale(28)
    readonly property int scaledEntryHeight: Theme.scale(22)
    readonly property int scaledDensitySlot: Theme.scale(28)
    readonly property int scaledOverflowSlot: Theme.scale(28)
    readonly property int scaledButton: Theme.scale(32)

    implicitHeight: scaledPathBarHeight
    objectName: "pathBar"
    color: Theme.surface1

    // Build display crumbs from the current path (string display only).
    // The root "/" is a single canonical crumb; separators are rendered only
    // between segments (never a doubled root slash — FIX-UI-002 / VIS-003).
    property var crumbs: []
    function rebuildCrumbs() {
        const parts = []
        const path = currentPath
        if (!path) {
            crumbs = parts
            return
        }
        parts.push({ label: "/", path: "/" })
        const segments = path.split("/").filter(function (s) { return s.length > 0 })
        let accumulated = ""
        for (let i = 0; i < segments.length; ++i) {
            accumulated += "/" + segments[i]
            parts.push({ label: segments[i], path: accumulated })
        }
        crumbs = parts
    }
    onCurrentPathChanged: rebuildCrumbs()
    Component.onCompleted: rebuildCrumbs()

    function beginEntry() {
        // Ctrl+L takes over the breadcrumb track; if search is active, leave
        // it first so the path editor owns the surface (no lost state).
        if (searchActive)
            exitSearch()
        entryError = ""
        entryMode = true
        entryField.text = currentPath
        entryField.visible = true
        entryField.forceActiveFocus()
        entryField.selectAll()
    }

    function commitEntry() {
        const target = entryField.text.trim()
        if (!controller) {
            return
        }
        if (controller.navigate(target)) {
            entryMode = false
            entryField.visible = false
            entryError = ""
        } else {
            entryError = qsTr("Folder not found")
        }
    }

    function activateCrumb(path) {
        if (controller) {
            controller.navigate(path)
        }
    }

    // Ctrl+L (location), Ctrl+O (order) and Ctrl+F (search) are declared once in
    // Main.qml's central keyboard block (REMEDIATION-012 / D-11) and reach this
    // component through its functions — no shortcut is declared here.

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.space2
        anchors.rightMargin: Theme.space2
        spacing: Theme.space1

        // --- Navigation group: back / forward / parent / refresh -------------
        ToolButton {
            id: backBtn
            objectName: "pathBarNavBack"
            implicitWidth: pathBar.scaledButton
            implicitHeight: pathBar.scaledButton
            ToolTip.text: qsTr("Back")
            ToolTip.visible: hovered
            Accessible.name: qsTr("Back")
            contentItem: Item {
                Rectangle {
                    anchors.fill: parent; anchors.margins: -4; radius: 6
                    color: "transparent"
                    border.color: backBtn.activeFocus ? (Theme.highContrast ? Theme.neonOutline : Theme.focus) : "transparent"
                    border.width: backBtn.activeFocus ? 2 : 0
                }
                Icon { anchors.centerIn: parent; name: "prev"; color: backBtn.enabled ? Theme.textHi : Theme.textLow; size: Theme.scale(16) }
            }
            enabled: controller ? controller.canGoBack : false
            onClicked: if (controller) controller.back()
        }
        ToolButton {
            id: forwardBtn
            objectName: "pathBarNavForward"
            implicitWidth: pathBar.scaledButton
            implicitHeight: pathBar.scaledButton
            ToolTip.text: qsTr("Forward")
            ToolTip.visible: hovered
            Accessible.name: qsTr("Forward")
            contentItem: Item {
                Rectangle {
                    anchors.fill: parent; anchors.margins: -4; radius: 6
                    color: "transparent"
                    border.color: forwardBtn.activeFocus ? (Theme.highContrast ? Theme.neonOutline : Theme.focus) : "transparent"
                    border.width: forwardBtn.activeFocus ? 2 : 0
                }
                Icon { anchors.centerIn: parent; name: "next"; color: forwardBtn.enabled ? Theme.textHi : Theme.textLow; size: Theme.scale(16) }
            }
            enabled: controller ? controller.canGoForward : false
            onClicked: if (controller) controller.forward()
        }
        ToolButton {
            id: parentBtn
            objectName: "pathBarNavParent"
            implicitWidth: pathBar.scaledButton
            implicitHeight: pathBar.scaledButton
            ToolTip.text: qsTr("Parent folder")
            ToolTip.visible: hovered
            Accessible.name: qsTr("Parent folder")
            contentItem: Item {
                Rectangle {
                    anchors.fill: parent; anchors.margins: -4; radius: 6
                    color: "transparent"
                    border.color: parentBtn.activeFocus ? (Theme.highContrast ? Theme.neonOutline : Theme.focus) : "transparent"
                    border.width: parentBtn.activeFocus ? 2 : 0
                }
                Icon { anchors.centerIn: parent; name: "parent"; color: parentBtn.enabled ? Theme.textHi : Theme.textLow; size: Theme.scale(16) }
            }
            // Disabled at the filesystem root (no parent exists) and when no
            // path is loaded. The controller owns the actual boundary handling.
            enabled: currentPath.length > 0 && currentPath !== "/"
            onClicked: if (controller) controller.parent()
        }
        ToolButton {
            id: refreshBtn
            objectName: "pathBarNavRefresh"
            implicitWidth: pathBar.scaledButton
            implicitHeight: pathBar.scaledButton
            ToolTip.text: qsTr("Refresh")
            ToolTip.visible: hovered
            Accessible.name: qsTr("Refresh")
            contentItem: Item {
                Rectangle {
                    anchors.fill: parent; anchors.margins: -4; radius: 6
                    color: "transparent"
                    border.color: refreshBtn.activeFocus ? (Theme.highContrast ? Theme.neonOutline : Theme.focus) : "transparent"
                    border.width: refreshBtn.activeFocus ? 2 : 0
                }
                Icon { anchors.centerIn: parent; name: "refresh"; color: refreshBtn.enabled ? Theme.textHi : Theme.textLow; size: Theme.scale(16) }
            }
            enabled: currentPath.length > 0
            onClicked: if (controller) controller.refresh()
        }

        // Breadcrumb track ⇄ path entry, in the same row. Hidden while search
        // is active so the search morph owns the fill-width surface.
        Item {
            objectName: "breadcrumbTrack"
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !pathBar.searchActive
            clip: true

            Row {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 0
                visible: !pathBar.entryMode

                Repeater {
                    model: pathBar.crumbs
                    delegate: Row {
                        spacing: 0

                        // Separator rendered only between segments (index > 0),
                        // so the root "/" crumb is never doubled (VIS-003).
                        Text {
                            objectName: "breadcrumbSeparator"
                            visible: index > 0
                            text: "\u203A"  // ›
                            color: Theme.textLow
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Rectangle {
                            objectName: "breadcrumbSegment"
                            color: segMouse.containsMouse ? Theme.surface3 : "transparent"
                            radius: Theme.radiusButton
                            implicitWidth: segText.implicitWidth + Theme.space2 * 2
                            implicitHeight: scaledChipHeight

                            Text {
                                id: segText
                                anchors.centerIn: parent
                                text: modelData.label
                                color: modelData.path === pathBar.currentPath ? Theme.textHi : Theme.textMid
                                font.pixelSize: Theme.fontBody
                            }

                            MouseArea {
                                id: segMouse
                                objectName: "breadcrumbClickArea"
                                anchors.fill: parent
                                hoverEnabled: true
                                onClicked: pathBar.activateCrumb(modelData.path)
                            }
                        }
                    }
                }
            }

            TextField {
                id: entryField
                objectName: "pathEntryField"
                visible: false
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                onAccepted: pathBar.commitEntry()
                Keys.onEscapePressed: {
                    pathBar.entryMode = false
                    pathBar.entryError = ""
                    visible = false
                }
            }

            Text {
                objectName: "pathEntryError"
                visible: pathBar.entryMode && pathBar.entryError.length > 0
                text: pathBar.entryError
                color: Theme.danger
                font.pixelSize: Theme.fontMeta
                anchors.left: parent.left
                anchors.top: entryField.bottom
            }
        }

        // --- Right-edge action cluster: search / order / density / overflow --
        ToolButton {
            id: searchBtn
            objectName: "pathSearchButton"
            implicitWidth: pathBar.scaledButton
            implicitHeight: pathBar.scaledButton
            ToolTip.text: qsTr("Search")
            ToolTip.visible: hovered
            Accessible.name: qsTr("Search")
            contentItem: Item {
                Rectangle {
                    anchors.fill: parent; anchors.margins: -4; radius: 6
                    color: "transparent"
                    border.color: searchBtn.activeFocus ? (Theme.highContrast ? Theme.neonOutline : Theme.focus) : "transparent"
                    border.width: searchBtn.activeFocus ? 2 : 0
                }
                Icon { anchors.centerIn: parent; name: "search"; color: Theme.textHi; size: Theme.scale(16) }
            }
            onClicked: pathBar.toggleSearch()
        }

        Item {
            id: searchMorphOverlay
            objectName: "breadcrumbTrack"  // inherits objectName for wiring
            visible: false
            Layout.fillWidth: true
            Layout.fillHeight: true
            implicitWidth: parent.width * 0.6
            implicitHeight: scaledChipHeight

            RowLayout {
                anchors.fill: parent
                spacing: Theme.space1

                // Scope chip: "This folder" (always active, Subfolders inert)
                Rectangle {
                    Layout.preferredWidth: scopeChipText.implicitWidth + Theme.space2 * 2
                    Layout.preferredHeight: scaledChipHeight
                    radius: Theme.radiusButton
                    color: Theme.surface3
                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        onClicked: pathBar.scopeToggled(!pathBar.searchSubfolders)
                    }
                    Text {
                        id: scopeChipText
                        anchors.centerIn: parent
                        text: pathBar.searchSubfolders ? "Subfolders" : "This folder"
                        color: pathBar.searchSubfolders ? Theme.textHi : Theme.textMid
                        font.pixelSize: Theme.fontMeta
                    }
                }
                TextField {
                    id: searchField
                    objectName: "pathSearchField"
                    Layout.fillWidth: true
                    placeholderText: qsTr("Search filename\u2026")
                    background: Rectangle {
                        color: Theme.surface0
                        radius: Theme.radiusButton
                        border.color: searchField.activeFocus ? (Theme.highContrast ? Theme.neonOutline : Theme.focus) : Theme.hairline
                        border.width: searchField.activeFocus ? 2 : 1
                    }
                    onTextChanged: pathBar.searchQueryChanged(text)
                    Keys.onEscapePressed: {
                        if (text.length > 0) {
                            text = ""  // first Esc clears query
                        } else {
                            pathBar.exitSearch()  // second Esc exits search mode
                        }
                    }
                }

                // Live result count
                Text {
                    id: searchCountText
                    objectName: "searchCount"
                    text: pathBar.searchResultCount > 0 ? pathBar.searchResultCount + " results" : ""
                    color: Theme.textMid
                    font.pixelSize: Theme.fontMeta
                    visible: text.length > 0
                }

                // × clear
                ToolButton {
                    contentItem: Icon {
                        name: "close"
                        color: Theme.textHi
                        size: Theme.scale(14)
                    }
                    onClicked: {
                        searchField.text = ""
                        pathBar.searchQueryChanged("")
                    }
                }

                // Cancel button
                Button {
                    text: qsTr("Cancel")
                    onClicked: pathBar.exitSearch()
                }
            }
        }

        ToolButton {
            id: orderBtn
            objectName: "pathOrderButton"
            implicitWidth: pathBar.scaledButton
            implicitHeight: pathBar.scaledButton
            ToolTip.text: qsTr("Order")
            ToolTip.visible: hovered
            Accessible.name: qsTr("Order")
            contentItem: Item {
                Rectangle {
                    anchors.fill: parent; anchors.margins: -4; radius: 6
                    color: "transparent"
                    border.color: orderBtn.activeFocus ? (Theme.highContrast ? Theme.neonOutline : Theme.focus) : "transparent"
                    border.width: orderBtn.activeFocus ? 2 : 0
                }
                Icon { anchors.centerIn: parent; name: "sort"; color: Theme.textHi; size: Theme.scale(16) }
            }
            onClicked: orderMenu.popup()
        }

        ToolButton {
            id: densityBtn
            objectName: "pathDensityButton"
            implicitWidth: pathBar.scaledButton
            implicitHeight: pathBar.scaledButton
            ToolTip.text: qsTr("Density: %1").arg(pathBar.densityLabel)
            ToolTip.visible: hovered
            Accessible.name: qsTr("View density: %1").arg(pathBar.densityLabel)
            contentItem: Item {
                Rectangle {
                    anchors.fill: parent; anchors.margins: -4; radius: 6
                    color: "transparent"
                    border.color: densityBtn.activeFocus ? (Theme.highContrast ? Theme.neonOutline : Theme.focus) : "transparent"
                    border.width: densityBtn.activeFocus ? 2 : 0
                }
                Icon {
                    objectName: "pathDensityIcon"
                    anchors.centerIn: parent
                    name: pathBar.densityIconName
                    color: Theme.textHi
                    size: Theme.scale(16)
                }
            }
            onClicked: densityMenu.popup()
        }

        ToolButton {
            id: overflowBtn
            objectName: "pathOverflowButton"
            implicitWidth: pathBar.scaledButton
            implicitHeight: pathBar.scaledButton
            ToolTip.text: qsTr("More")
            ToolTip.visible: hovered
            Accessible.name: qsTr("More")
            contentItem: Item {
                Rectangle {
                    anchors.fill: parent; anchors.margins: -4; radius: 6
                    color: "transparent"
                    border.color: overflowBtn.activeFocus ? (Theme.highContrast ? Theme.neonOutline : Theme.focus) : "transparent"
                    border.width: overflowBtn.activeFocus ? 2 : 0
                }
                Icon { anchors.centerIn: parent; name: "more"; color: Theme.textHi; size: Theme.scale(16) }
            }
            onClicked: overflowMenu.popup()
        }
    }

    // Order menu (DESIGN.md §5.7, §3.2): each key + direction + folders-first.
    Menu {
        id: orderMenu

        MenuItem {
            objectName: "orderHeader"
            text: qsTr("Sort by")
            enabled: false
        }
        MenuItem {
            objectName: "orderName"
            text: qsTr("Name")
            checked: pathBar.sortKey === 0
            onTriggered: pathBar.selectOrder(0, pathBar.sortDescending, pathBar.foldersFirst)
        }
        MenuItem {
            objectName: "orderModified"
            text: qsTr("Modified")
            checked: pathBar.sortKey === 1
            onTriggered: pathBar.selectOrder(1, pathBar.sortDescending, pathBar.foldersFirst)
        }
        MenuItem {
            objectName: "orderDuration"
            text: qsTr("Duration")
            checked: pathBar.sortKey === 2
            onTriggered: pathBar.selectOrder(2, pathBar.sortDescending, pathBar.foldersFirst)
        }
        MenuItem {
            objectName: "orderSize"
            text: qsTr("Size")
            checked: pathBar.sortKey === 3
            onTriggered: pathBar.selectOrder(3, pathBar.sortDescending, pathBar.foldersFirst)
        }
        MenuItem {
            objectName: "orderWatched"
            text: qsTr("Watched")
            checked: pathBar.sortKey === 4
            onTriggered: pathBar.selectOrder(4, pathBar.sortDescending, pathBar.foldersFirst)
        }
        MenuItem {
            objectName: "orderFavorite"
            text: qsTr("Favorite")
            checked: pathBar.sortKey === 5
            onTriggered: pathBar.selectOrder(5, pathBar.sortDescending, pathBar.foldersFirst)
        }
        MenuSeparator {}
        MenuItem {
            objectName: "orderDescending"
            text: qsTr("Descending")
            checked: pathBar.sortDescending
            onTriggered: pathBar.selectOrder(pathBar.sortKey, !pathBar.sortDescending,
                                              pathBar.foldersFirst)
        }
        MenuItem {
            objectName: "orderFoldersFirst"
            text: qsTr("Folders first")
            checked: pathBar.foldersFirst
            onTriggered: pathBar.selectOrder(pathBar.sortKey, pathBar.sortDescending,
                                              !pathBar.foldersFirst)
        }
    }

    // Density menu (DESIGN.md §3.2, §3.5): Gallery / Standard / Compact,
    // matching the ViewDensity enum (0/1/2). Per-folder persistence lives in
    // the controller — this menu only forwards the chosen mode.
    Menu {
        id: densityMenu
        objectName: "densityMenu"

        MenuItem {
            objectName: "densityGallery"
            text: qsTr("Gallery")
            checkable: true
            checked: pathBar.density === 0
            onTriggered: pathBar.selectDensity(0)
        }
        MenuItem {
            objectName: "densityStandard"
            text: qsTr("Standard")
            checkable: true
            checked: pathBar.density === 1
            onTriggered: pathBar.selectDensity(1)
        }
        MenuItem {
            objectName: "densityCompact"
            text: qsTr("Compact")
            checkable: true
            checked: pathBar.density === 2
            onTriggered: pathBar.selectDensity(2)
        }
    }

    // Overflow menu (DESIGN.md §23.2): secondary commands. "Show hidden files"
    // is the documented overflow command; it forwards to the controller's
    // setShowHidden (per-folder persistence owned by the controller).
    Menu {
        id: overflowMenu
        objectName: "overflowMenu"

        MenuItem {
            objectName: "overflowShowHidden"
            text: qsTr("Show hidden files")
            checkable: true
            checked: pathBar.showHidden
            onTriggered: pathBar.toggleShowHidden()
        }
    }
}
