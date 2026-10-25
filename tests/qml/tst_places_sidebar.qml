// PlacesSidebar tests (Prompt 15; icons, capability-gated device actions,
// group headings as model rows and long-list reachability added by FIX-UI-003).

import QtQuick
import QtQuick.Controls
import QtTest
import qg

Item {
    id: testRoot
    width: 640
    height: 640

TestCase {
    name: "PlacesSidebar"
    when: windowShown
    width: parent.width
    height: parent.height

    // Mirrors the PlacesModel contract: group headings are ordinary rows
    // (isHeader) interleaved with the locations of each group.
    ListModel {
        id: placesFixture
        ListElement { label: "PINNED"; path: ""; icon: ""; kind: "header"; group: 0; isHeader: true; connected: true; removable: false; ejectable: false; mounted: true; canUnmount: false; canEject: false; volumeId: ""; tooltip: "" }
        ListElement { label: "Projects"; path: "/home/u/Projects"; icon: "pin"; kind: "pinned"; group: 0; isHeader: false; connected: true; removable: false; ejectable: false; mounted: true; canUnmount: false; canEject: false; volumeId: ""; tooltip: "/home/u/Projects" }
        ListElement { label: "PLACES"; path: ""; icon: ""; kind: "header"; group: 1; isHeader: true; connected: true; removable: false; ejectable: false; mounted: true; canUnmount: false; canEject: false; volumeId: ""; tooltip: "" }
        ListElement { label: "Home"; path: "/home/u"; icon: "home"; kind: "place"; group: 1; isHeader: false; connected: true; removable: false; ejectable: false; mounted: true; canUnmount: false; canEject: false; volumeId: ""; tooltip: "/home/u" }
        ListElement { label: "Videos"; path: "/home/u/Videos"; icon: "videos"; kind: "place"; group: 1; isHeader: false; connected: true; removable: false; ejectable: false; mounted: true; canUnmount: false; canEject: false; volumeId: ""; tooltip: "/home/u/Videos" }
        ListElement { label: "A very long place label that must be truncated rather than push the device action away"; path: "/home/u/Long"; icon: "documents"; kind: "place"; group: 1; isHeader: false; connected: true; removable: false; ejectable: false; mounted: true; canUnmount: false; canEject: false; volumeId: ""; tooltip: "/home/u/Long" }
        ListElement { label: "DEVICES"; path: ""; icon: ""; kind: "header"; group: 2; isHeader: true; connected: true; removable: false; ejectable: false; mounted: true; canUnmount: false; canEject: false; volumeId: ""; tooltip: "" }
        ListElement { label: "Data"; path: "/mnt/data"; icon: "drive"; kind: "internal"; group: 2; isHeader: false; connected: true; removable: false; ejectable: false; mounted: true; canUnmount: false; canEject: false; volumeId: "/mnt/data"; tooltip: "/mnt/data" }
        ListElement { label: "USB Stick"; path: "/media/usb"; icon: "drive-removable"; kind: "removable"; group: 2; isHeader: false; connected: true; removable: true; ejectable: true; mounted: true; canUnmount: true; canEject: true; volumeId: "/media/usb"; tooltip: "/media/usb" }
        ListElement { label: "Storage 1 Blue"; path: "/mnt/storage/blue"; icon: "drive-removable"; kind: "removable"; group: 2; isHeader: false; connected: true; removable: true; ejectable: false; mounted: true; canUnmount: true; canEject: false; volumeId: "/mnt/storage/blue"; tooltip: "/mnt/storage/blue" }
        ListElement { label: "NAS"; path: "/mnt/nas"; icon: "network"; kind: "network"; group: 2; isHeader: false; connected: true; removable: false; ejectable: false; mounted: true; canUnmount: true; canEject: false; volumeId: "/mnt/nas"; tooltip: "/mnt/nas" }
        ListElement { label: "Old HDD"; path: "/media/old"; icon: "drive-removable"; kind: "removable"; group: 2; isHeader: false; connected: false; removable: true; ejectable: false; mounted: false; canUnmount: false; canEject: false; volumeId: "/media/old"; tooltip: "Old HDD — not connected" }
        ListElement { label: "RECENT"; path: ""; icon: ""; kind: "header"; group: 3; isHeader: true; connected: true; removable: false; ejectable: false; mounted: true; canUnmount: false; canEject: false; volumeId: ""; tooltip: "" }
        ListElement { label: "Archive"; path: "/home/u/Archive"; icon: "recent"; kind: "recent"; group: 3; isHeader: false; connected: true; removable: false; ejectable: false; mounted: true; canUnmount: false; canEject: false; volumeId: ""; tooltip: "/home/u/Archive" }
    }

    ListModel { id: longFixture }

    Component {
        id: sidebarComponent
        PlacesSidebar {}
    }

    function findByName(item, name) {
        if (!item)
            return null
        if (item.objectName === name)
            return item
        const kids = item.children
        if (!kids)
            return null
        for (let i = 0; i < kids.length; ++i) {
            const found = findByName(kids[i], name)
            if (found)
                return found
        }
        return null
    }

    function makeSidebar(model, expanded, height) {
        const wide = expanded === undefined ? true : expanded
        return createTemporaryObject(sidebarComponent, testRoot,
                                     { model: model, expanded: wide,
                                       width: wide ? 240 : 56, height: height || 560 })
    }

    function listOf(sidebar) {
        return findByName(sidebar, "placesList")
    }

    function rowForPath(list, path) {
        for (let i = 0; i < list.count; ++i) {
            const row = list.itemAtIndex(i)
            if (row && row.rowPath === path)
                return row
        }
        return null
    }

    function appendRow(model, label, path, icon, kind, group, isHeader, connected,
                       removable, ejectable, canUnmount, canEject) {
        model.append({ label: label, path: path, icon: icon, kind: kind, group: group,
                       isHeader: isHeader, connected: connected, removable: removable,
                       ejectable: ejectable, mounted: connected, canUnmount: canUnmount,
                       canEject: canEject, volumeId: path, tooltip: path })
    }

    function test_groupsAndRowsRender() {
        const sidebar = makeSidebar(placesFixture)
        verify(sidebar !== null)
        verify(findByName(sidebar, "sectionHeaderLabel") !== null, "group heading rendered")
        verify(findByName(sidebar, "placeRow") !== null, "location row rendered")
    }

    // FIX-UI-003-G: the four documented groups appear, in order, as headings.
    function test_sectionHeadingsUseDesignLabelsInOrder() {
        const sidebar = makeSidebar(placesFixture)
        const list = listOf(sidebar)
        let headings = []
        for (let i = 0; i < list.count; ++i) {
            const row = list.itemAtIndex(i)
            if (row && row.rowIsHeader)
                headings.push(findByName(row, "sectionHeaderLabel").text)
        }
        compare(headings.length, 4, "one heading per non-empty group")
        compare(headings[0], "PINNED")
        compare(headings[1], "PLACES")
        compare(headings[2], "DEVICES")
        compare(headings[3], "RECENT")
    }

    function test_locationActivationEmitsPath() {
        const sidebar = makeSidebar(placesFixture)
        let activated = ""
        sidebar.locationActivated.connect(function (path) { activated = path })

        const area = findByName(sidebar, "placeRowClickArea")
        verify(area !== null)
        sidebar.activateLocation("/home/u")
        compare(activated, "/home/u")
    }

    function test_disconnectedBadgeAndRailMode() {
        const sidebar = makeSidebar(placesFixture, false)
        verify(sidebar.implicitWidth === sidebar.railWidth)
        verify(findByName(sidebar, "placeRow") !== null)
    }

    // VIS-005: every location row draws the icon the model published — the
    // previous delegate hardcoded one generic "device" glyph for all rows.
    function test_rowsRenderModelIconAndLabel() {
        const sidebar = makeSidebar(placesFixture)
        const list = listOf(sidebar)

        const expected = { "/home/u/Projects": "pin", "/home/u": "home",
                           "/home/u/Videos": "videos", "/home/u/Long": "documents",
                           "/mnt/data": "drive", "/media/usb": "drive-removable",
                           "/mnt/storage/blue": "drive-removable", "/mnt/nas": "network",
                           "/media/old": "drive-removable", "/home/u/Archive": "recent" }

        let checked = 0
        for (let i = 0; i < list.count; ++i) {
            const row = list.itemAtIndex(i)
            if (!row || row.rowIsHeader)
                continue
            const icon = findByName(row, "placeRowIcon")
            const label = findByName(row, "placeRowLabel")
            verify(icon !== null, "row " + i + " has an icon")
            verify(label !== null, "row " + i + " has a label")
            verify(icon.visible, "icon visible on a location row")
            compare(icon.name, expected[row.rowPath], "icon for " + row.rowPath)
            verify(icon.fileName !== "", "icon resolves to a shipped SVG")
            verify(label.text.length > 0, "label populated")
            ++checked
        }
        verify(checked >= 8, "all location rows checked: " + checked)
    }

    // Headings are not locations: no icon, no click, no focus ring.
    function test_headingsAreNotInteractive() {
        const sidebar = makeSidebar(placesFixture)
        const list = listOf(sidebar)
        for (let i = 0; i < list.count; ++i) {
            const row = list.itemAtIndex(i)
            if (!row || !row.rowIsHeader)
                continue
            compare(findByName(row, "placeRowIcon").visible, false)
            compare(findByName(row, "placeRowClickArea").enabled, false)
            verify(row.rowPath === "", "heading carries no navigation target")
            verify(row.rowCurrent === false, "heading never takes the current-row ring")
        }
    }

    // VIS-006: disconnected devices keep their identity, grey out and badge.
    function test_disconnectedDeviceShowsBadgeAndTooltip() {
        const sidebar = makeSidebar(placesFixture)
        const list = listOf(sidebar)
        const row = rowForPath(list, "/media/old")
        verify(row !== null, "disconnected row rendered")
        verify(row.rowDisconnected, "identity preserved and marked disconnected")
        const badge = findByName(row, "deviceDisconnectedBadge")
        verify(badge !== null)
        verify(badge.visible, "amber badge visible")
        verify(row.rowHasAction === false, "no action for a disconnected volume")
    }

    // FIX-UI-003-F: the affordance follows the reported capability, and eject is
    // distinguishable from unmount.
    function test_deviceActionFollowsCapability() {
        const sidebar = makeSidebar(placesFixture)
        const list = listOf(sidebar)

        const ejectRow = rowForPath(list, "/media/usb")
        const unmountRow = rowForPath(list, "/mnt/storage/blue")
        const plainRow = rowForPath(list, "/mnt/data")
        verify(ejectRow !== null && unmountRow !== null && plainRow !== null)

        verify(ejectRow.rowHasAction === true)
        verify(ejectRow.rowCanEject === true)
        compare(findByName(ejectRow, "deviceActionIcon").name, "eject")
        compare(findByName(ejectRow, "deviceActionButton").Accessible.name, "Eject USB Stick")

        // Unmount-only device (the desktop reports can_unmount, not can_eject).
        verify(unmountRow.rowHasAction === true)
        verify(unmountRow.rowCanEject === false)
        compare(findByName(unmountRow, "deviceActionIcon").name, "unmount")
        compare(findByName(unmountRow, "deviceActionButton").Accessible.name,
                "Unmount Storage 1 Blue")

        // A fixed internal volume reports no desktop action at all.
        verify(plainRow.rowHasAction === false)
    }

    // The action must be clickable: previously the full-row MouseArea covered
    // the button, so the eject affordance never received a click.
    function test_deviceActionEmitsEjectAndUnmount() {
        const sidebar = makeSidebar(placesFixture)
        const list = listOf(sidebar)

        let ejectRequested = ""
        let unmountRequested = ""
        sidebar.volumeEjectRequested.connect(function (id) { ejectRequested = id })
        sidebar.volumeUnmountRequested.connect(function (id) { unmountRequested = id })

        const ejectRow = rowForPath(list, "/media/usb")
        const unmountRow = rowForPath(list, "/mnt/storage/blue")
        verify(ejectRow !== null && unmountRow !== null)
        const ejectButton = findByName(ejectRow, "deviceActionButton")
        const unmountButton = findByName(unmountRow, "deviceActionButton")

        // Reveal on hover (DESIGN.md §3.3) …
        mouseMove(ejectRow, ejectRow.width / 2, ejectRow.height / 2)
        tryCompare(ejectButton, "visible", true)
        // … then click the affordance itself, not the row.
        mouseClick(ejectButton)
        compare(ejectRequested, "/media/usb")
        compare(unmountRequested, "")

        mouseMove(unmountRow, unmountRow.width / 2, unmountRow.height / 2)
        tryCompare(unmountButton, "visible", true)
        mouseClick(unmountButton)
        compare(unmountRequested, "/mnt/storage/blue")
        compare(ejectRequested, "/media/usb")
    }

    // Long labels elide and can never widen the row or hide the device action.
    function test_longLabelIsTruncated() {
        const sidebar = makeSidebar(placesFixture)
        const list = listOf(sidebar)
        const row = rowForPath(list, "/home/u/Long")
        verify(row !== null)
        const label = findByName(row, "placeRowLabel")
        compare(label.elide, Text.ElideRight)
        verify(label.width < row.width, "label is bounded by the row")
        const slot = findByName(row, "deviceActionSlot")
        verify(label.x + label.width <= slot.x + 1, "label never overlaps the action slot")
    }

    // Pinned and Recent must stay reachable even with a long Devices list, and
    // the scroll range must cover the very last row (exact content height).
    function test_longDeviceListKeepsPinnedAndRecentReachable() {
        longFixture.clear()
        appendRow(longFixture, "PINNED", "", "", "header", 0, true, true, false, false, false, false)
        appendRow(longFixture, "Projects", "/home/u/Projects", "pin", "pinned", 0, false, true, false, false, false, false)
        appendRow(longFixture, "PLACES", "", "", "header", 1, true, true, false, false, false, false)
        appendRow(longFixture, "Home", "/home/u", "home", "place", 1, false, true, false, false, false, false)
        appendRow(longFixture, "DEVICES", "", "", "header", 2, true, true, false, false, false, false)
        for (let i = 0; i < 40; ++i) {
            appendRow(longFixture, "Volume " + i, "/mnt/vol" + i, "drive", "internal", 2,
                      false, true, false, false, false, false)
        }
        appendRow(longFixture, "RECENT", "", "", "header", 3, true, true, false, false, false, false)
        appendRow(longFixture, "Videos Recent", "/home/u/Videos", "recent", "recent", 3,
                  false, true, false, false, false, false)

        const sidebar = makeSidebar(longFixture, true, 600)
        const list = listOf(sidebar)
        compare(list.count, longFixture.count)

        // The whole sidebar scrolls, so nothing is permanently out of reach.
        verify(list.contentHeight > list.height, "content exceeds the viewport and scrolls")

        // No artificial cap on devices, and — because headings are rows of the
        // same height — the content height is exact rather than estimated, so
        // the scroll range really covers the last row.
        verify(list.count === longFixture.count, "every row is present")
        compare(list.contentHeight, list.count * sidebar.scaledRowHeight)

        // Scrolling to the end must bring the Recent row fully on screen.
        list.positionViewAtEnd()
        let last = null
        for (let i = 0; i < 40 && last === null; ++i) {
            wait(50)
            last = list.itemAtIndex(list.count - 1)
        }
        verify(last !== null, "recent row instantiated after scrolling to the end")
        compare(findByName(last, "placeRowIcon").name, "recent")
        // A delegate's y is in content coordinates, so map through contentY.
        const lastTop = last.y - list.contentY
        verify(lastTop >= 0 && lastTop + last.height <= list.height + 1,
               "recent row is fully inside the viewport after scrolling")
        verify(list.contentY > 0, "the view actually scrolled")

        // And back to the top for Pinned.
        list.positionViewAtBeginning()
        let pinned = null
        for (let i = 0; i < 40 && pinned === null; ++i) {
            wait(50)
            pinned = list.itemAtIndex(1) // index 0 is the PINNED heading
        }
        verify(pinned !== null)
        tryCompare(list, "contentY", 0)
        const pinnedTop = pinned.y - list.contentY
        verify(pinnedTop >= 0 && pinnedTop + pinned.height <= list.height + 1,
               "pinned row is fully inside the viewport at the top")
        compare(findByName(pinned, "placeRowIcon").name, "pin")
    }

    // Keyboard operability: Tab reaches the list, arrows move the current row
    // while skipping headings, Enter activates, and the focus ring is visible.
    function test_keyboardNavigationSkipsHeadingsAndKeepsFocusRing() {
        const sidebar = makeSidebar(placesFixture)
        const list = listOf(sidebar)
        let activated = ""
        sidebar.locationActivated.connect(function (path) { activated = path })

        list.forceActiveFocus()
        wait(50)
        verify(list.activeFocus, "sidebar list can take keyboard focus")

        // Start on the pinned row (index 1); index 2 is the PLACES heading and
        // must be skipped in both directions.
        list.currentIndex = 1
        sidebar.moveCurrent(1)
        compare(list.currentIndex, 3, "heading skipped moving down")
        sidebar.moveCurrent(-1)
        compare(list.currentIndex, 1, "heading skipped moving up")

        const row = list.itemAtIndex(1)
        tryCompare(row, "rowCurrent", true)
        const ring = findByName(row, "placeRowFocusRing")
        verify(ring !== null)
        verify(ring.visible, "focus ring visible on the current row")

        sidebar.activateCurrent()
        compare(activated, "/home/u/Projects")
    }

    // Rail mode shows icons only — the icon still resolves and a tooltip tells
    // the user what the row is; headings become dividers.
    function test_railModeKeepsIconsAndTooltips() {
        const sidebar = makeSidebar(placesFixture, false)
        const list = listOf(sidebar)
        const row = rowForPath(list, "/home/u")
        verify(row !== null)
        const icon = findByName(row, "placeRowIcon")
        compare(icon.name, "home")
        verify(icon.fileName !== "")
        compare(findByName(row, "placeRowLabel").visible, false)

        const heading = rowForPath(list, "")
        verify(heading !== null)
        compare(findByName(heading, "sectionHeaderLabel").visible, false)
        verify(findByName(heading, "sectionHeaderDivider").visible)
    }

    // Pinning travels through the model, not a second data source.
    function test_pinIntentIsEmitted() {
        const sidebar = makeSidebar(placesFixture)
        let pinned = ""
        let unpinned = ""
        sidebar.pinRequested.connect(function (path) { pinned = path })
        sidebar.unpinRequested.connect(function (path) { unpinned = path })

        sidebar.pinRequested("/home/u/New")
        sidebar.unpinRequested("/home/u/Projects")
        compare(pinned, "/home/u/New")
        compare(unpinned, "/home/u/Projects")
    }
}
}
