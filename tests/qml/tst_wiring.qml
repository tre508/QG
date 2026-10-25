// Shell wiring tests (Prompt 16).
//
// Verifies Main.qml binds the injected controller/model/service to the region
// components and forwards their intent back to the controller.

import QtQuick
import QtQuick.Controls
import QtTest
import qg

TestCase {
    name: "Wiring"
    when: windowShown

    component MockController: QtObject {
        signal errorOccurred(string path, string message)
        property bool busy: false
        property string currentPath: "/"
        property string folderName: "Summer trip"
        property string orderLabel: "Name \u2191"
        property bool canGoBack: false
        property bool canGoForward: false
        property var navigations: []
        function back() {}
        function forward() {}
        function parent() {}
        function refresh() {}
        function navigate(p) { navigations.push(p); return true }
    }

    component MockMounts: QtObject {
        property var ejectCalls: []
        function eject(id) { ejectCalls.push(id) }
    }

    // Mock folder model exposing the identity accessor shape Main.qml calls
    // (REMEDIATION-002 / D-05): identityAt must exist and be callable from QML.
    component MockFolderModel: QtObject {
        property var identityCalls: []
        property string identityForRow: "inode:1:202"
        property string name: "clip.mp4"
        function identityAt(row) { identityCalls.push(row); return identityForRow }
    }

    component MockPlaybackController: QtObject {
        // Signal names must exist on the mock for Main.qml's on*() handlers.
        signal playbackStarted(string path)
        signal playbackEnded()
        signal endOfFolder()
        property var playCalls: []
        property bool cinemaActive: false
        property bool playing: false
        property var currentSession: null
        property int reelFilter: 0
        property string positionLabel: ""
        property bool canGoPrevious: false
        property bool canGoNext: false
        function play(path, identity) { playCalls.push({ path: path, identity: identity }) }
        function stop() {}
        function enterCinema() {}
        function exitCinema() {}
        function toggleCinema() {}
        function previous() {}
        function next() {}
        function setReelFilter(value) { reelFilter = value }
        function selectReelRow(row) {}
        function currentIdentity() { return "" }
    }

    // Search pipeline mocks (REMEDIATION-004 / D-03). Shape matches the real
    // SearchController/SearchProxy API used by Main.qml.
    component MockSearchController: QtObject {
        signal activeChanged(bool active)
        signal resultsChanged(string summary)
        property string appliedQuery: ""
        property string folderName: ""
        property string sortLabel: ""
        property var queries: []
        function setQuery(q) { appliedQuery = q; queries.push(q) }
        function clear() { appliedQuery = "" }
        function query() { return appliedQuery }
        function summary() { return appliedQuery.length ? ("results for " + appliedQuery) : "" }
        function setFolderName(n) { folderName = n }
        function setSortLabel(l) { sortLabel = l }
    }

    component MockSearchProxy: QtObject {
        property int resultCount: 0
    }

    // SelectionState mock (REMEDIATION-013 / D-12). Tracks selection by identity.
    component MockSelectionState: QtObject {
        property var selected: []
        property int focusIndexVal: 0
        property int anchorIndexVal: 0
        signal selectionChanged()
        signal focusChanged()
        signal selectionSummaryChanged()
        function isSelected(id) { return selected.indexOf(id) !== -1 }
        function selectedCount() { return selected.length }
        function selectedIdentities() { return selected }
        function focusIndex() { return focusIndexVal }
        function setFocusIndex(idx) { focusIndexVal = idx; focusChanged() }
        function anchorIndex() { return anchorIndexVal }
        function setAnchorIndex(idx) { anchorIndexVal = idx }
        function selectOne(id, idx) { selected = [id]; focusIndexVal = idx; anchorIndexVal = idx; selectionChanged(); focusChanged(); selectionSummaryChanged() }
        function toggle(id, idx) { const i = selected.indexOf(id); if (i === -1) selected.push(id); else selected.splice(i, 1); focusIndexVal = idx; selectionChanged(); focusChanged(); selectionSummaryChanged() }
        function extendRange(id, idx) { selected.push(id); focusIndexVal = idx; selectionChanged(); focusChanged(); selectionSummaryChanged() }
        function selectAll() { selectionChanged(); selectionSummaryChanged() }
        function clear() { selected = []; focusIndexVal = -1; anchorIndexVal = -1; selectionChanged(); focusChanged(); selectionSummaryChanged() }
        function selectionSummary() { return selected.length === 0 ? "No selection" : selected.length + " selected" }
    }

    // Thumbnail / probe viewport mocks (REM-01): Main.qml routes the
    // ContentView's visible model range to both schedulers.
    component MockThumbnailController: QtObject {
        property var visibleCalls: []
        function requestVisible(first, last) { visibleCalls.push({ first: first, last: last }) }
    }

    component MockProbeController: QtObject {
        property var visibleCalls: []
        function requestVisible(first, last) { visibleCalls.push({ first: first, last: last }) }
    }

    Component { id: mockControllerComponent; MockController {} }
    Component { id: mockThumbnailControllerComponent; MockThumbnailController {} }
    Component { id: mockProbeControllerComponent; MockProbeController {} }
    Component { id: mockMountsComponent; MockMounts {} }
    Component { id: mockFolderModelComponent; MockFolderModel {} }
    Component { id: mockPlaybackControllerComponent; MockPlaybackController {} }
    Component { id: mockSearchControllerComponent; MockSearchController {} }
    Component { id: mockSearchProxyComponent; MockSearchProxy {} }
    Component { id: mockSelectionStateComponent; MockSelectionState {} }
    Component { id: mainComponent; Main {} }

    ListModel {
        id: folderFixture
        ListElement { name: "sub"; path: "/tmp/sub"; isFolder: true }
        ListElement { name: "clip.mp4"; path: "/tmp/clip.mp4"; isFolder: false }
    }

    ListModel {
        id: placesFixture
        ListElement { label: "Home"; path: "/home/u"; icon: "user-home"; group: 1; connected: true; removable: false; ejectable: false; volumeId: "" }
    }

    function rootOf(shell) {
        return shell.contentItem ? shell.contentItem : shell
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

    function makeShell() {
        const controller = mockControllerComponent.createObject(null)
        const mounts = mockMountsComponent.createObject(null)
        const selectionState = mockSelectionStateComponent.createObject(null)
        const shell = createTemporaryObject(mainComponent, null, {
            browserController: controller,
            folderModel: folderFixture,
            placesModel: placesFixture,
            mounts: mounts,
            selectionState: selectionState
        })
        return { shell: shell, controller: controller, mounts: mounts, selectionState: selectionState }
    }

    function test_shellBindsInjectedControllerAndModel() {
        const ctx = makeShell()
        verify(ctx.shell !== null)

        const pathBar = findByName(rootOf(ctx.shell), "pathBar")
        verify(pathBar !== null)
        verify(pathBar.controller === ctx.controller)

        const content = findByName(rootOf(ctx.shell), "contentView")
        verify(content !== null)
        verify(content.model === folderFixture)
    }

    function test_pathBarActionReachesController() {
        const ctx = makeShell()
        const pathBar = findByName(rootOf(ctx.shell), "pathBar")
        pathBar.activateCrumb("/navigate")
        compare(ctx.controller.navigations[ctx.controller.navigations.length - 1], "/navigate")
    }

    function test_placesActivationAndEjectReachController() {
        const ctx = makeShell()
        const sidebar = findByName(rootOf(ctx.shell), "placesSidebar")
        verify(sidebar !== null)

        sidebar.activateLocation("/home/u")
        compare(ctx.controller.navigations[ctx.controller.navigations.length - 1], "/home/u")

        sidebar.volumeEjectRequested("/media/usb")
        compare(ctx.mounts.ejectCalls[0], "/media/usb")
    }

    function test_contentActivationNavigatesFolderOnly() {
        const ctx = makeShell()
        const content = findByName(rootOf(ctx.shell), "contentView")
        verify(content !== null)

        content.activatedItem(0, "/tmp/sub", true)
        compare(ctx.controller.navigations[ctx.controller.navigations.length - 1], "/tmp/sub")

        // A video activation must not navigate in P16 (playback is later).
        const before = ctx.controller.navigations.length
        content.activatedItem(1, "/tmp/clip.mp4", false)
        compare(ctx.controller.navigations.length, before)
    }

    function test_videoActivationCallsIdentityAndPlayback() {
        // D-05 smoke: Main.qml must be able to call folderModel.identityAt(row)
        // without a missing-method error, and pass the identity to playback.
        const controller = mockControllerComponent.createObject(null)
        const folder = mockFolderModelComponent.createObject(null)
        const playback = mockPlaybackControllerComponent.createObject(null)
        const shell = createTemporaryObject(mainComponent, null, {
            browserController: controller,
            folderModel: folder,
            playbackController: playback,
            placesModel: placesFixture
        })
        verify(shell !== null)

        const content = findByName(rootOf(shell), "contentView")
        verify(content !== null)
        content.activatedItem(1, "/tmp/clip.mp4", false)

        compare(folder.identityCalls.length, 1, "identityAt invoked from QML")
        compare(folder.identityCalls[0], 1)
        compare(playback.playCalls.length, 1, "play invoked with identity")
        compare(playback.playCalls[0].path, "/tmp/clip.mp4")
        compare(playback.playCalls[0].identity, "inode:1:202")
    }

    function test_statusStripReflectsViewSelection() {
        const ctx = makeShell()
        // The test uses MockFolderModel which provides identityAt
        const folder = mockFolderModelComponent.createObject(null)
        const shell = createTemporaryObject(mainComponent, null, {
            browserController: ctx.controller,
            folderModel: folder,
            placesModel: placesFixture,
            mounts: ctx.mounts,
            selectionState: ctx.selectionState
        })
        const status = findByName(rootOf(shell), "statusStrip")
        verify(status !== null)
        compare(status.selectedCount, 0)
        compare(findByName(status, "statusSelection").text, "No selection")

        const content = findByName(rootOf(shell), "contentView")
        content.selectIndex(1)
        compare(status.selectedCount, 1)
        verify(findByName(status, "statusSelection").text.indexOf("1 selected") === 0)
    }

    function test_searchMorphDrivesControllerAndManifest() {
        // D-03: Ctrl+F field → SearchController → Manifest presentation.
        const controller = mockControllerComponent.createObject(null)
        const search = mockSearchControllerComponent.createObject(null)
        const proxy = mockSearchProxyComponent.createObject(null)
        proxy.resultCount = 2
        const shell = createTemporaryObject(mainComponent, null, {
            browserController: controller,
            folderModel: folderFixture,
            placesModel: placesFixture,
            searchController: search,
            searchProxy: proxy
        })
        verify(shell !== null)

        const pathBar = findByName(rootOf(shell), "pathBar")
        const content = findByName(rootOf(shell), "contentView")
        verify(pathBar !== null && content !== null)

        // Live count binding comes from the proxy.
        compare(pathBar.searchResultCount, 2)

        // Typing in the morph field reaches the controller.
        pathBar.searchQueryChanged("sun")
        compare(search.appliedQuery, "sun")

        // Controller becoming active swaps the surface into Manifest.
        search.activeChanged(true)
        compare(content.manifest, true)
        verify(content.searchProxy === proxy, "manifest bound to the search proxy")
        compare(content.searchQuery, "sun")

        // Result summary is surfaced on the content surface.
        search.resultsChanged("2 results for \"sun\" in Summer trip")
        compare(content.summaryText, "2 results for \"sun\" in Summer trip")
    }

    function test_searchClearRestoresBrowseWithoutStaleBinding() {
        const controller = mockControllerComponent.createObject(null)
        const search = mockSearchControllerComponent.createObject(null)
        const proxy = mockSearchProxyComponent.createObject(null)
        const shell = createTemporaryObject(mainComponent, null, {
            browserController: controller,
            folderModel: folderFixture,
            placesModel: placesFixture,
            searchController: search,
            searchProxy: proxy
        })
        verify(shell !== null)

        const content = findByName(rootOf(shell), "contentView")
        verify(content !== null)

        // Enter search, then cancel/clear.
        search.activeChanged(true)
        compare(content.manifest, true)
        compare(content.searchProxy, proxy)

        search.clear()
        search.activeChanged(false)
        compare(content.manifest, false)
        verify(content.searchProxy === null, "no stale model binding after clear")
        compare(content.searchQuery, "")
        compare(content.summaryText, "")
    }

    function test_viewportRangeReachesThumbnailAndProbeControllers() {
        // REM-01: ContentView reports its visible canonical-model row range and
        // Main.qml routes it to both the thumbnail and probe schedulers.
        const controller = mockControllerComponent.createObject(null)
        const thumbs = mockThumbnailControllerComponent.createObject(null)
        const probes = mockProbeControllerComponent.createObject(null)
        const shell = createTemporaryObject(mainComponent, null, {
            browserController: controller,
            folderModel: folderFixture,
            placesModel: placesFixture,
            thumbnailController: thumbs,
            probeController: probes
        })
        verify(shell !== null)

        const content = findByName(rootOf(shell), "contentView")
        verify(content !== null)

        // The view reports during layout; reset so we measure the explicit action.
        thumbs.visibleCalls = []
        probes.visibleCalls = []

        content.visibleRangeChanged(2, 5)
        compare(thumbs.visibleCalls.length, 1, "thumbnail controller received the range")
        compare(thumbs.visibleCalls[0].first, 2)
        compare(thumbs.visibleCalls[0].last, 5)
        compare(probes.visibleCalls.length, 1, "probe controller received the range")
        compare(probes.visibleCalls[0].first, 2)
        compare(probes.visibleCalls[0].last, 5)

        // A second viewport report (scroll) reaches the controllers again.
        content.visibleRangeChanged(6, 9)
        compare(thumbs.visibleCalls.length, 2)
        compare(probes.visibleCalls.length, 2)
    }

    function test_contentViewReportsRangeWhenPopulated() {
        // REM-01: a populated model makes the view report its initial range.
        const controller = mockControllerComponent.createObject(null)
        const thumbs = mockThumbnailControllerComponent.createObject(null)
        const probes = mockProbeControllerComponent.createObject(null)
        const shell = createTemporaryObject(mainComponent, null, {
            browserController: controller,
            folderModel: folderFixture,
            placesModel: placesFixture,
            thumbnailController: thumbs,
            probeController: probes
        })
        verify(shell !== null)

        const content = findByName(rootOf(shell), "contentView")
        verify(content !== null)
        // Reset (layout already reported), then drive the report explicitly
        // (offscreen layout has no real viewport to scroll).
        thumbs.visibleCalls = []
        probes.visibleCalls = []
        content.reportVisibleRange()

        compare(thumbs.visibleCalls.length, 1, "initial range reported to thumbnails")
        compare(probes.visibleCalls.length, 1, "initial range reported to probes")
        // The fixture has 2 rows, so the reported range is within [0, 1].
        verify(thumbs.visibleCalls[0].first === 0, "range starts at the first row")
        verify(thumbs.visibleCalls[0].last <= 1, "range ends within the model")
    }
}
