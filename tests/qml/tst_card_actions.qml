// Card action wiring tests (REMEDIATION-009 / D-04, D-25).
//
// Two layers:
//   * ContentView: the delegate's card signals forward with the row index, the
//     keyboard file-action keys dispatch, the inline-rename target is exposed,
//     and a double-click burst activates exactly once.
//   * Main: every forwarded intent reaches a composed controller (play → playback,
//     favorite/watched → state, rename/reveal/open-with/properties → actions,
//     retry → thumbnails) and the overflow control opens the context menu whose
//     dispatch is a real, testable function.

import QtQuick
import QtQuick.Controls
import QtTest
import qg

TestCase {
    id: testRoot
    name: "CardActions"
    when: windowShown

    // --- ContentView-only fixture -------------------------------------------
    ListModel {
        id: cardFixture
        ListElement { name: "a.mp4"; path: "/v/a.mp4"; isFolder: false }
        ListElement { name: "b.mp4"; path: "/v/b.mp4"; isFolder: false }
    }

    Component { id: contentComponent; ContentView {} }

    // --- Main-level mocks ----------------------------------------------------
    component MockFolderModel: QtObject {
        property string identityForRow: "inode:1:202"
        property string pathForRow: "/tmp/clip.mp4"
        // Role-shaped values so the (row-less) mock does not log assignment
        // warnings when a delegate binds model.<role>.
        property string name: "clip.mp4"
        property string path: "/tmp/clip.mp4"
        property bool isFolder: false
        property int size: 4096
        property int mtime: 0
        function identityAt(row) { return identityForRow }
        function pathAt(row) { return pathForRow }
        function isFolderAt(row) { return false }
    }

    component MockPlaybackController: QtObject {
        signal playbackStarted(string path)
        signal playbackEnded()
        signal endOfFolder()
        property bool cinemaActive: false
        property bool playing: true
        property var currentSession: null
        property int reelFilter: 0
        property string positionLabel: ""
        property bool canGoPrevious: false
        property bool canGoNext: false
        property var playCalls: []
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

    component MockState: QtObject {
        property var favoriteCalls: []
        property var watchedCalls: []
        function toggleFavorite(identity) { favoriteCalls.push(identity); return true }
        function toggleWatched(identity) { watchedCalls.push(identity); return true }
    }

    component MockActions: QtObject {
        signal actionFinished(string kind, bool success)
        signal toastRequested(string message, string undoKind, string undoPath)
        property var renameCalls: []
        property var revealCalls: []
        property var openWithCalls: []
        property var propertiesCalls: []
        property var copyCalls: []
        property var cutCalls: []
        property var trashCalls: []
        property var copyPathCalls: []
        property var undoLastTrashCalls: []
        property var fileInfoCalls: []
        // REMEDIATION-010 additions: dialogs ask these before acting.
        property bool renameTargetExists: false
        property string renameTarget: "/tmp/clip (renamed).mp4"
        property string uniqueRenameBase: "renamed (2)"
        function menuItemsForSelection(count) {
            return ["Play", "Favorite", "Mark Watched", "Rename", "Cut", "Copy",
                    "Reveal", "Open With", "Add to Collection (coming soon)",
                    "Properties", "Move to Trash", "Delete Permanently"]
        }
        function rename(path, newName) { renameCalls.push({ path: path, newName: newName }) }
        function reveal(path) { revealCalls.push(path) }
        function openWith(path) { openWithCalls.push(path) }
        function properties(path) { propertiesCalls.push(path) }
        function copyToClipboard(paths) { copyCalls.push(paths) }
        function cut(paths) { cutCalls.push(paths) }
        function trashOne(path) { trashCalls.push(path) }
        function copyPath(path) { copyPathCalls.push(path) }
        function targetPathForRename(path, base) { return renameTarget }
        function pathExists(path) { return renameTargetExists }
        function uniqueBaseName(path, base) { return uniqueRenameBase }
        function fileInfo(path) {
            fileInfoCalls.push(path)
            return { path: path, name: "clip.mp4", exists: true, sizeBytes: 2048,
                     mtimeNs: 1700000000000000000, permissions: "rw-r--r--",
                     owner: "tester", mimeType: "video/mp4", isDir: false }
        }
        function undoLastTrash() { undoLastTrashCalls.push(true); return true }
    }

    component MockThumbnails: QtObject {
        property var retryCalls: []
        function retry(identity) { retryCalls.push(identity) }
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

    Component { id: mainComponent; Main {} }
    Component { id: folderModelComponent; MockFolderModel {} }
    Component { id: playbackComponent; MockPlaybackController {} }
    Component { id: stateComponent; MockState {} }
    Component { id: actionsComponent; MockActions {} }
    Component { id: thumbnailsComponent; MockThumbnails {} }
    Component { id: selectionStateComponent; MockSelectionState {} }

    ListModel {
        id: placesFixture
        ListElement { label: "Home"; path: "/home/u"; icon: "user-home"; group: 1; connected: true; removable: false; ejectable: false; volumeId: "" }
    }

    function collectByName(item, name, out) {
        if (!item)
            return out
        if (item.objectName === name)
            out.push(item)
        const kids = item.children
        if (!kids)
            return out
        for (let i = 0; i < kids.length; ++i)
            collectByName(kids[i], name, out)
        return out
    }

    function findByName(item, name) {
        if (!item)
            return null
        if (item.objectName === name)
            return item
        const lists = [item.children, item.data, item.resources]
        for (let l = 0; l < lists.length; ++l) {
            const kids = lists[l]
            if (!kids)
                continue
            for (let i = 0; i < kids.length; ++i) {
                const found = findByName(kids[i], name)
                if (found)
                    return found
            }
        }
        return null
    }

    function menuItemsOf(menu, name) {
        let out = collectByName(menu.contentItem, name, [])
        if (out.length === 0)
            out = collectByName(menu, name, [])
        return out
    }

    function rootOf(shell) {
        return shell.contentItem ? shell.contentItem : shell
    }

    function makeView() {
        const selectionState = selectionStateComponent.createObject(null)
        const view = createTemporaryObject(contentComponent, testRoot,
                                           { width: 1000, height: 600, model: cardFixture, selectionState: selectionState })
        verify(view !== null)
        return { view: view, selectionState: selectionState }
    }

    function buildMain() {
        const folder = folderModelComponent.createObject(null)
        const playback = playbackComponent.createObject(null)
        const state = stateComponent.createObject(null)
        const actions = actionsComponent.createObject(null)
        const thumbs = thumbnailsComponent.createObject(null)
        const selectionState = selectionStateComponent.createObject(null)
        const shell = createTemporaryObject(mainComponent, null, {
            folderModel: folder,
            placesModel: placesFixture,
            playbackController: playback,
            stateController: state,
            actionsController: actions,
            thumbnailController: thumbs,
            selectionState: selectionState
        })
        verify(shell !== null)
        return { shell: shell, folder: folder, playback: playback, state: state,
                 actions: actions, thumbs: thumbs, selectionState: selectionState,
                 content: findByName(rootOf(shell), "contentView") }
    }

    // --- ContentView: the delegate forwards card signals with the row index ---
    function test_delegateCardSignalsForwardWithIndex() {
        const { view } = makeView()
        const cells = collectByName(view, "contentCell", [])
        verify(cells.length >= 2, "cards instantiated")

        let fav = [], retry = [], overflow = [], rename = [], play = []
        view.favoriteToggled.connect(function (i) { fav.push(i) })
        view.thumbnailRetryRequested.connect(function (i) { retry.push(i) })
        view.overflowRequested.connect(function (i, x, y) { overflow.push({ i: i, x: x, y: y }) })
        view.renameCommitted.connect(function (i, n) { rename.push({ i: i, n: n }) })
        view.activatedItem.connect(function (i) { play.push(i) })

        // Emit the card's own signals — exactly what the buttons do.
        cells[1].favoriteRequested()
        cells[1].thumbnailRetryRequested()
        cells[0].playRequested()
        cells[1].overflowRequested()
        cells[1].renameCommitted("renamed.mp4")

        compare(fav.length, 1)
        compare(fav[0], 1)
        compare(retry.length, 1)
        compare(retry[0], 1)
        compare(play.length, 1)
        compare(play[0], 0)
        compare(overflow.length, 1)
        compare(overflow[0].i, 1)
        compare(rename.length, 1)
        compare(rename[0].i, 1)
        compare(rename[0].n, "renamed.mp4")
    }

    // --- ContentView: one activation per double-click burst ------------------
    function test_doubleClickDoesNotPlayTwice() {
        const { view } = makeView()
        let activations = []
        view.activatedItem.connect(function (i) { activations.push(i) })

        // A click (select) followed by the double-click activation.
        view.selectIndex(1)
        view.activateIndex(1)
        view.activateIndex(1)   // still the same burst → suppressed
        compare(activations.length, 1)
        compare(activations[0], 1)

        // A deliberate activation of another row still works.
        view.activateIndex(0)
        compare(activations.length, 2)
        compare(activations[1], 0)
    }

    // --- ContentView: keyboard file actions ---------------------------------
    function test_keyboardDispatchesFocusedCardActions() {
        const { view, selectionState } = makeView()
        selectionState.setFocusIndex(1)
        let fav = [], watched = []
        view.favoriteToggled.connect(function (i) { fav.push(i) })
        view.watchedToggled.connect(function (i) { watched.push(i) })

        // Ctrl+Shift+F → favorite on the focused row.
        view.handleKey({ key: Qt.Key_F, modifiers: Qt.ControlModifier | Qt.ShiftModifier, accepted: false })
        compare(fav.length, 1)
        compare(fav[0], 1)

        // Ctrl+Shift+W → watched on the focused row.
        view.handleKey({ key: Qt.Key_W, modifiers: Qt.ControlModifier | Qt.ShiftModifier, accepted: false })
        compare(watched.length, 1)
        compare(watched[0], 1)

        // F2 → inline rename on the focused row.
        view.handleKey({ key: Qt.Key_F2, modifiers: Qt.NoModifier, accepted: false })
        compare(view.renameIndex, 1)
        const cells = collectByName(view, "contentCell", [])
        compare(cells[1].renaming, true)
    }

    // --- ContentView: rename target begins and clears ------------------------
    function test_inlineRenameTargetLifecycle() {
        const { view } = makeView()
        let commits = []
        view.renameCommitted.connect(function (i, n) { commits.push({ i: i, n: n }) })

        view.beginRename(0)
        compare(view.renameIndex, 0)
        const cells = collectByName(view, "contentCell", [])
        compare(cells[0].renaming, true)
        compare(cells[1].renaming, false)

        // The delegate clears the target before forwarding the commit.
        cells[0].renameCommitted("new.mp4")
        compare(view.renameIndex, -1)
        compare(commits.length, 1)
        compare(commits[0].i, 0)
        compare(commits[0].n, "new.mp4")
    }

    // --- Main: forwarded intents reach the composed controllers --------------
    function test_mainRoutesIntentsToControllers() {
        const ctx = buildMain()
        const content = ctx.content
        verify(content !== null)

        content.playRequested(1)
        compare(ctx.playback.playCalls.length, 1)
        compare(ctx.playback.playCalls[0].path, "/tmp/clip.mp4")
        compare(ctx.playback.playCalls[0].identity, "inode:1:202")

        content.favoriteToggled(1)
        compare(ctx.state.favoriteCalls.length, 1)
        compare(ctx.state.favoriteCalls[0], "inode:1:202")

        content.watchedToggled(1)
        compare(ctx.state.watchedCalls.length, 1)
        compare(ctx.state.watchedCalls[0], "inode:1:202")

        content.renameCommitted(1, "renamed")
        compare(ctx.actions.renameCalls.length, 1)
        compare(ctx.actions.renameCalls[0].path, "/tmp/clip.mp4")
        compare(ctx.actions.renameCalls[0].newName, "renamed")

        content.thumbnailRetryRequested(1)
        compare(ctx.thumbs.retryCalls.length, 1)
        compare(ctx.thumbs.retryCalls[0], "inode:1:202")
    }

    // --- Main: overflow opens the context menu, dispatch is real -------------
    function test_overflowOpensMenuAndDispatchRoutes() {
        const ctx = buildMain()
        const menu = findByName(rootOf(ctx.shell), "contentMenu")
        verify(menu !== null)

        ctx.content.overflowRequested(1, 40, 60)
        compare(menu.targetIndex, 1)

        ctx.shell.dispatchMenuAction("Favorite")
        compare(ctx.state.favoriteCalls.length, 1)
        compare(ctx.state.favoriteCalls[0], "inode:1:202")

        ctx.shell.dispatchMenuAction("Mark Watched")
        compare(ctx.state.watchedCalls.length, 1)

        ctx.shell.dispatchMenuAction("Reveal")
        compare(ctx.actions.revealCalls.length, 1)
        compare(ctx.actions.revealCalls[0], "/tmp/clip.mp4")

        // Properties opens QG's dialog, populated from real stats (REM-010).
        ctx.shell.dispatchMenuAction("Properties")
        compare(ctx.actions.fileInfoCalls.length, 1)
        const props = findByName(rootOf(ctx.shell), "propertiesDialog")
        verify(props !== null)
        compare(props.visible, true)
        compare(props.fileName, "clip.mp4")
        compare(props.size.length > 0, true)

        ctx.shell.dispatchMenuAction("Copy")
        compare(ctx.actions.copyCalls.length, 1)

        // Rename opens the inline editor on the target row (there are no rows in
        // this mock model, so the rename target stays unset — the beginRename
        // path itself is asserted in the ContentView-level test above).
        ctx.shell.dispatchMenuAction("Rename")
        compare(ctx.content.renameIndex, -1)
        compare(ctx.actions.renameCalls.length, 0)

        // Trash runs without a confirmation (it is undoable); permanent delete
        // always confirms (DESIGN.md §11.1).
        const confirm = findByName(rootOf(ctx.shell), "confirmDeleteDialog")
        verify(confirm !== null)
        ctx.shell.dispatchMenuAction("Move to Trash")
        compare(ctx.actions.trashCalls.length, 1)
        compare(ctx.actions.trashCalls[0], "/tmp/clip.mp4")
        compare(confirm.visible, false)

        ctx.shell.dispatchMenuAction("Delete Permanently")
        compare(confirm.visible, true)
        compare(ctx.actions.trashCalls.length, 1) // nothing destructive ran yet
    }

    // --- Main: a failed action surfaces instead of vanishing ---------------
    function test_actionFailureSurfacesAsToast() {
        const ctx = buildMain()
        const toast = findByName(rootOf(ctx.shell), "toastHost")
        verify(toast !== null)
        compare(toast.message.length, 0)

        // Success is silent.
        ctx.actions.actionFinished("copy", true)
        compare(toast.message.length, 0)

        // Failure is visible.
        ctx.actions.actionFinished("rename", false)
        verify(toast.message.indexOf("rename") !== -1)
    }

    function test_menuIsPopulatedAndFutureItemDisabled() {
        const ctx = buildMain()
        const menu = findByName(rootOf(ctx.shell), "contentMenu")
        verify(menu !== null)
        ctx.content.overflowRequested(0, 0, 0)
        verify(menu.count >= 12)

        const items = menuItemsOf(menu, "menuItem")
        const separators = menuItemsOf(menu, "menuSeparator")
        // Verbs are grouped ahead of the destructive pair, which comes last.
        compare(separators.length, 3)
        compare(items[items.length - 1].text.indexOf("Delete Permanently") !== -1, true)
        compare(items[items.length - 2].text.indexOf("Move to Trash") !== -1, true)
        compare(items[items.length - 1].destructive, true)

        // The collection entry is explicitly a disabled future item.
        let disabledCollection = false
        for (let i = 0; i < items.length; ++i) {
            if (items[i].text.indexOf("Collection") !== -1 && !items[i].enabled)
                disabledCollection = true
        }
        verify(disabledCollection, "Add to Collection is disabled")
    }
}
