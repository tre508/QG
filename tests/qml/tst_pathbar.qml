// PathBar component tests (Prompt 14, extended by FIX-UI-002).
//
// Uses a BrowserController-shaped mock; the real controller is wired in P16.
// FIX-UI-002 adds coverage for the restored four-button navigation group, the
// single-root breadcrumb (no doubled slash), and the Density/Overflow actions.

import QtQuick
import QtQuick.Controls
import QtTest
import qg

TestCase {
    name: "PathBar"
    when: windowShown

    component MockController: QtObject {
        property string currentPath: "/home/user/Videos"
        property bool canGoBack: false
        property bool canGoForward: false
        property var navigations: []
        property int backCount: 0
        property int sortKey: 0
        property bool sortDescending: false
        property bool foldersFirst: true
        property int density: 1
        property bool showHidden: false
        property var setSortCalls: []
        property var setDensityCalls: []
        property var setShowHiddenCalls: []
        function back() { backCount++ }
        function forward() {}
        function parent() {}
        function refresh() {}
        function navigate(p) { navigations.push(p); return p !== "/bad" }
        function setSort(key, desc, ff) { setSortCalls.push([key, desc, ff]) }
        function setDensityValue(d) { setDensityCalls.push(d) }
        function setShowHidden(s) { setShowHiddenCalls.push(s) }
    }

    Component {
        id: mockComponent
        MockController {}
    }

    Component {
        id: pathBarComponent
        PathBar {}
    }

    function makeController() {
        return mockComponent.createObject(null)
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

    function test_navButtonsDriveController() {
        const controller = makeController()
        controller.canGoBack = true
        const bar = createTemporaryObject(pathBarComponent, null, { controller: controller })
        verify(bar !== null)

        const back = findByName(bar, "pathBarNavBack")
        verify(back !== null)
        compare(back.enabled, true)
        back.clicked()
        compare(controller.backCount, 1)

        const forward = findByName(bar, "pathBarNavForward")
        compare(forward.enabled, false)
    }

    // FIX-UI-002 / VIS-002: all four navigation controls exist and are visible
    // at wide width, with the correct themed icons.
    function test_navGroupCompleteAndVisible() {
        const controller = makeController()
        const bar = createTemporaryObject(pathBarComponent, null, { controller: controller })
        verify(bar !== null)

        const back = findByName(bar, "pathBarNavBack")
        const forward = findByName(bar, "pathBarNavForward")
        const parent = findByName(bar, "pathBarNavParent")
        const refresh = findByName(bar, "pathBarNavRefresh")
        verify(back !== null && forward !== null && parent !== null && refresh !== null)
        verify(back.visible && forward.visible && parent.visible && refresh.visible)

        // Correct themed icons (not the old chevron/more fallbacks).
        compare(back.contentItem.children[1].name, "prev")
        compare(forward.contentItem.children[1].name, "next")
        compare(parent.contentItem.children[1].name, "parent")
        compare(refresh.contentItem.children[1].name, "refresh")
    }

    // FIX-UI-002: Parent is disabled at the filesystem root (no parent exists).
    function test_parentDisabledAtRoot() {
        const controller = makeController()
        controller.currentPath = "/"
        const bar = createTemporaryObject(pathBarComponent, null, { controller: controller })
        const parent = findByName(bar, "pathBarNavParent")
        verify(parent !== null)
        compare(parent.enabled, false)
    }

    // FIX-UI-002 / VIS-003: the root "/" appears exactly once and separators
    // are "›" (never a doubled slash).
    function test_breadcrumbRootAppearsOnce() {
        const controller = makeController()
        controller.currentPath = "/home/user"
        const bar = createTemporaryObject(pathBarComponent, null, { controller: controller })
        // crumbs: "/", "home", "user"
        compare(bar.crumbs.length, 3)
        compare(bar.crumbs[0].label, "/")
        compare(bar.crumbs[0].path, "/")
        // Exactly one crumb resolves to the root.
        let rootCount = 0
        for (let i = 0; i < bar.crumbs.length; ++i)
            if (bar.crumbs[i].path === "/")
                rootCount++
        compare(rootCount, 1)
    }

    // FIX-UI-002 / VIS-003: the separator glyph is "›", not "/".
    function test_breadcrumbSeparatorIsChevron() {
        const controller = makeController()
        controller.currentPath = "/home/user"
        const bar = createTemporaryObject(pathBarComponent, null, { controller: controller })
        const sep = findByName(bar, "breadcrumbSeparator")
        verify(sep !== null)
        compare(sep.text, "\u203A")
    }

    function test_breadcrumbsNavigate() {
        const controller = makeController()
        const bar = createTemporaryObject(pathBarComponent, null, { controller: controller })
        // "/", "home", "user", "Videos"
        compare(bar.crumbs.length, 4)

        const segment = findByName(bar, "breadcrumbSegment")
        verify(segment !== null)
        bar.activateCrumb("/")
        compare(controller.navigations[0], "/")
    }

    function test_ctrlLPathEntryValidation() {
        const controller = makeController()
        const bar = createTemporaryObject(pathBarComponent, null, { controller: controller })

        bar.beginEntry()
        verify(bar.entryMode)
        const field = findByName(bar, "pathEntryField")
        verify(field.visible)

        field.text = "/bad"
        bar.commitEntry()
        const error = findByName(bar, "pathEntryError")
        verify(error.visible)

        field.text = "/good"
        bar.commitEntry()
        verify(!bar.entryMode)
        compare(controller.navigations[controller.navigations.length - 1], "/good")
    }

    function test_orderMenuReadsSortStateFromController() {
        const controller = makeController()
        controller.sortKey = 3
        controller.sortDescending = true
        controller.foldersFirst = false
        const bar = createTemporaryObject(pathBarComponent, null, { controller: controller })
        compare(bar.sortKey, 3)
        compare(bar.sortDescending, true)
        compare(bar.foldersFirst, false)
    }

    function test_orderMenuEmitsOrderChanged() {
        const bar = createTemporaryObject(pathBarComponent, null, {})
        let got = null
        bar.orderChanged.connect(function (key, desc, ff) { got = [key, desc, ff] })
        bar.selectOrder(2, false, true)
        verify(got !== null)
        compare(got[0], 2)
        compare(got[1], false)
        compare(got[2], true)
    }

    function test_orderButtonPresent() {
        const bar = createTemporaryObject(pathBarComponent, null, {})
        verify(findByName(bar, "pathOrderButton") !== null)
    }

    // FIX-UI-002 / VIS-004: the Density button exists, is visible, and its
    // selection forwards through the densityChanged signal.
    function test_densityButtonAndMenu() {
        const controller = makeController()
        const bar = createTemporaryObject(pathBarComponent, null, { controller: controller })
        const densityBtn = findByName(bar, "pathDensityButton")
        verify(densityBtn !== null)
        verify(densityBtn.visible)

        let got = null
        bar.densitySelected.connect(function (d) { got = d })
        bar.selectDensity(2)
        compare(got, 2)
    }

    // FIX-UI-002 / VIS-004: the Overflow button exists, is visible, and the
    // show-hidden toggle forwards through the showHiddenToggled signal.
    function test_overflowButtonAndMenu() {
        const controller = makeController()
        const bar = createTemporaryObject(pathBarComponent, null, { controller: controller })
        const overflowBtn = findByName(bar, "pathOverflowButton")
        verify(overflowBtn !== null)
        verify(overflowBtn.visible)

        let got = null
        bar.showHiddenToggled.connect(function (s) { got = s })
        bar.toggleShowHidden()
        compare(got, true)
    }
}
