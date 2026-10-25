// REMEDIATION-010 (D-08, D-28, D-31): context menu shape, dialogs, toasts and
// the browser content-state overlay.
//
// Layers under test:
//   * menu structure from ActionsController::menuItemsForSelection — dividers,
//     disabled future item, destructive pair last, multi-selection title +
//     bulk-only verbs;
//   * RenameConflictDialog / PropertiesDialog / ConfirmDeleteDialog opening from
//     their real triggers and routing a real action or a truthful state;
//   * ToastHost feedback + the Undo affordance routed to the controller;
//   * StateOverlay precedence (disconnected > access denied > error > loading >
//     empty) and suppression when content is present.

import QtQuick
import QtQuick.Controls
import QtTest
import qg

TestCase {
    id: testRoot
    name: "OverlaysDialogs"
    when: windowShown

    // --- Fixtures ------------------------------------------------------------
    ListModel {
        id: cardFixture
        ListElement { name: "a.mp4"; path: "/v/a.mp4"; isFolder: false }
        ListElement { name: "b.mp4"; path: "/v/b.mp4"; isFolder: false }
    }

    component MockFolderModel: QtObject {
        property int count: 0
        property bool favorite: false
        property bool watched: false
        property string identityForRow: "inode:5:202"
        property string pathForRow: "/v/a.mp4"
        property string name: "a.mp4"
        property string path: "/v/a.mp4"
        property bool isFolder: false
        property int size: 4096
        function identityAt(row) { return identityForRow }
        function pathAt(row) { return pathForRow }
        function isFolderAt(row) { return false }
        function favoriteAt(row) { return favorite }
        function watchedAt(row) { return watched }
    }

    component MockBrowser: QtObject {
        signal errorOccurred(string path, string message)
        property string currentPath: "/videos"
        property string folderName: "videos"
        property bool canGoBack: false
        property bool canGoForward: false
        property bool busy: false
        property int refreshes: 0
        property var navigations: []
        function refresh() { refreshes += 1 }
        function navigate(path) { navigations.push(path) }
        function homePath() { return "/home/tester" }
    }

    component MockMounts: QtObject {
        property bool disconnected: false
        property string disconnectedDrive: "usb stick"
        property string volumePath: "/media/usb"
        function isInsideDisconnectedVolume(path) { return path.indexOf(volumePath) === 0 }
    }

    component MockSelection: QtObject {
        property int count: 1
        function selectedCount() { return count }
        function selectedIdentities() { return [] }
    }

    component MockPlayback: QtObject {
        signal playbackStarted(string path)
        signal playbackEnded()
        signal endOfFolder()
        property bool cinemaActive: false
        property bool playing: false
        property var currentSession: null
        property int reelFilter: 0
        property string positionLabel: ""
        property bool canGoPrevious: false
        property bool canGoNext: false
        function play(path, identity) {}
        function stop() {}
        function enterCinema() {}
        function exitCinema() {}
        function toggleCinema() {}
        function previous() {}
        function next() {}
        function setReelFilter(value) {}
        function selectReelRow(row) {}
        function currentIdentity() { return "" }
    }

    component MockState: QtObject {
        function toggleFavorite(identity) { return true }
        function toggleWatched(identity) { return true }
    }

    component MockThumbnails: QtObject {
        function retry(identity) {}
    }

    component MockActions: QtObject {
        signal actionFinished(string kind, bool success)
        signal toastRequested(string message, string undoKind, string undoPath)
        property var renameCalls: []
        property var trashCalls: []
        property var copyPathCalls: []
        property var undoLastTrashCalls: []
        property var fileInfoCalls: []
        property var deleteCalls: []
        property bool renameTargetExists: false
        property string renameTarget: "/v/a (renamed).mp4"
        property string uniqueRenameBase: "renamed (2)"
        function menuItemsForSelection(count) {
            const items = ["Play", "Favorite", "Mark Watched", "Rename", "Cut", "Copy",
                           "Reveal", "Open With", "Add to Collection (coming soon)",
                           "Properties", "Move to Trash", "Delete Permanently"]
            if (count > 1)
                items.unshift("" + count + " items")
            return items
        }
        function rename(path, newName) { renameCalls.push({ path: path, newName: newName }) }
        function trashOne(path) { trashCalls.push(path) }
        function copyPath(path) { copyPathCalls.push(path) }
        function targetPathForRename(path, base) { return renameTarget }
        function pathExists(path) { return renameTargetExists }
        function uniqueBaseName(path, base) { return uniqueRenameBase }
        function fileInfo(path) {
            fileInfoCalls.push(path)
            return { path: path, name: "a.mp4", exists: true, sizeBytes: 2048,
                     mtimeNs: 1700000000000000000, permissions: "rw-r--r--",
                     owner: "tester", mimeType: "video/mp4", isDir: false }
        }
        function undoLastTrash() { undoLastTrashCalls.push(true); return true }
        function deletePermanently(paths) { deleteCalls.push(paths) }
    }

    ListModel {
        id: placesFixture
        ListElement { label: "Home"; path: "/home/tester"; icon: "user-home"; group: 1; connected: true; removable: false; ejectable: false; volumeId: "" }
    }

    Component { id: mainComponent; Main {} }
    Component { id: contentComponent; ContentView {} }

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

    // Imperative Menu items live under the menu's contentItem; fall back to the
    // menu object itself so the assertion does not depend on Controls internals.
    function menuItemsOf(menu, name) {
        let out = collectByName(menu.contentItem, name, [])
        if (out.length === 0)
            out = collectByName(menu, name, [])
        return out
    }

    function rootOf(shell) {
        return shell.contentItem ? shell.contentItem : shell
    }

    Component { id: folderComponent; MockFolderModel {} }
    Component { id: browserComponent; MockBrowser {} }
    Component { id: mountsComponent; MockMounts {} }
    Component { id: selectionComponent; MockSelection {} }
    Component { id: playbackComponent; MockPlayback {} }
    Component { id: stateComponent; MockState {} }
    Component { id: actionsComponent; MockActions {} }
    Component { id: thumbnailsComponent; MockThumbnails {} }

    // Fresh mocks + shell. `count` seeds folderModel.count (drives the overlay).
    function makeContext(count) {
        const folder = createTemporaryObject(folderComponent, null, { count: count === undefined ? 0 : count })
        const browser = createTemporaryObject(browserComponent, null, {})
        const mounts = createTemporaryObject(mountsComponent, null, {})
        const selection = createTemporaryObject(selectionComponent, null, {})
        const playback = createTemporaryObject(playbackComponent, null, {})
        const state = createTemporaryObject(stateComponent, null, {})
        const actions = createTemporaryObject(actionsComponent, null, {})
        const thumbs = createTemporaryObject(thumbnailsComponent, null, {})
        const shell = createTemporaryObject(mainComponent, null, {
            folderModel: folder,
            placesModel: placesFixture,
            browserController: browser,
            mountsController: mounts,
            selectionState: selection,
            playbackController: playback,
            stateController: state,
            actionsController: actions,
            thumbnailController: thumbs
        })
        verify(shell !== null)
        return {
            shell: shell, folder: folder, browser: browser, mounts: mounts,
            selection: selection, playback: playback, state: state, actions: actions,
            content: findByName(rootOf(shell), "contentView"),
            toast: findByName(rootOf(shell), "toastHost"),
            overlay: findByName(rootOf(shell), "contentStateOverlay"),
            menu: findByName(rootOf(shell), "contentMenu"),
            props: findByName(rootOf(shell), "propertiesDialog"),
            conflict: findByName(rootOf(shell), "renameConflictDialog"),
            confirm: findByName(rootOf(shell), "confirmDeleteDialog")
        }
    }

    // --- Context menu structure (D-08) --------------------------------------
    function test_menuGroupsVerbsBeforeDestructivePair() {
        const ctx = makeContext(3)
        verify(ctx.menu !== null)
        ctx.content.overflowRequested(0, 10, 20)
        compare(ctx.menu.targetIndex, 0)

        const items = menuItemsOf(ctx.menu, "menuItem")
        const separators = menuItemsOf(ctx.menu, "menuSeparator")
        compare(separators.length, 3, "verbs | browse verbs | properties | destructive")

        // Destructive actions sit last, after the final divider.
        compare(items[items.length - 1].text.indexOf("Delete Permanently") !== -1, true)
        compare(items[items.length - 2].text.indexOf("Move to Trash") !== -1, true)
        compare(items[items.length - 1].destructive, true)
        compare(items[items.length - 1].enabled, true)

        // Add to Collection is present but a disabled future verb.
        let collection = null
        for (let i = 0; i < items.length; ++i)
            if (items[i].text.indexOf("Collection") !== -1)
                collection = items[i]
        verify(collection !== null)
        compare(collection.enabled, false)

        // Shortcut gutter carries the documented key.
        let renameItem = null
        for (let i = 0; i < items.length; ++i)
            if (items[i].text.indexOf("Rename") !== -1)
                renameItem = items[i]
        compare(renameItem.shortcutLabel, "F2")
    }

    function test_menuMultiSelectionShowsTitleAndBulkVerbsOnly() {
        const ctx = makeContext(3)
        ctx.selection.count = 3
        ctx.content.overflowRequested(0, 0, 0)

        const items = menuItemsOf(ctx.menu, "menuItem")
        let title = null
        const labels = []
        for (let i = 0; i < items.length; ++i) {
            labels.push(items[i].text)
            if (items[i].text === "3 items")
                title = items[i]
        }
        verify(title !== null, "N-items title row")
        compare(title.enabled, false)

        // Single-item verbs disappear; bulk verbs survive.
        compare(labels.join("|").indexOf("Play") !== -1, false)
        compare(labels.join("|").indexOf("Rename") !== -1, false)
        compare(labels.join("|").indexOf("Properties") !== -1, false)
        compare(labels.join("|").indexOf("Favorite") !== -1, true)
        compare(labels.join("|").indexOf("Move to Trash") !== -1, true)
        compare(menuItemsOf(ctx.menu, "menuSeparator").length, 2)
    }

    // --- Rename conflict (D-28) ---------------------------------------------
    function test_renameCollisionOpensConflictDialogAndKeepBothRenames() {
        const ctx = makeContext(1)
        ctx.actions.renameTargetExists = true
        ctx.content.renameCommitted(0, "renamed")

        verify(ctx.conflict !== null)
        compare(ctx.conflict.visible, true)
        compare(ctx.conflict.targetName, "a (renamed).mp4")
        compare(ctx.actions.renameCalls.length, 0, "nothing renamed while unresolved")

        ctx.conflict.keepBothConfirmed()
        compare(ctx.conflict.visible, false)
        compare(ctx.actions.renameCalls.length, 1)
        compare(ctx.actions.renameCalls[0].newName, "renamed (2)")
    }

    function test_renameWithoutCollisionRenamesDirectly() {
        const ctx = makeContext(1)
        ctx.actions.renameTargetExists = false
        ctx.content.renameCommitted(0, "renamed")
        compare(ctx.conflict.visible, false)
        compare(ctx.actions.renameCalls.length, 1)
        compare(ctx.actions.renameCalls[0].newName, "renamed")
    }

    function test_renameConflictCancelLeavesFileUntouched() {
        const ctx = makeContext(1)
        ctx.actions.renameTargetExists = true
        ctx.content.renameCommitted(0, "renamed")
        compare(ctx.conflict.visible, true)
        ctx.conflict.cancelConfirmed()
        compare(ctx.conflict.visible, false)
        compare(ctx.actions.renameCalls.length, 0)
        compare(ctx.shell.pendingRenamePath, "")
    }

    function test_renameConflictOverwriteReportsMissingCapability() {
        const ctx = makeContext(1)
        ctx.actions.renameTargetExists = true
        ctx.content.renameCommitted(0, "renamed")
        ctx.conflict.overwriteConfirmed()
        compare(ctx.conflict.visible, false)
        compare(ctx.actions.renameCalls.length, 0, "no silent overwrite")
        verify(ctx.toast.message.indexOf("Overwrite") !== -1)
    }

    // --- Properties (D-28) ---------------------------------------------------
    function test_propertiesDialogShowsRealStatsAndFlags() {
        const ctx = makeContext(1)
        ctx.folder.favorite = true
        ctx.folder.watched = false
        ctx.content.overflowRequested(0, 0, 0)   // arms the menu target row
        ctx.shell.dispatchMenuAction("Properties")

        compare(ctx.actions.fileInfoCalls.length, 1, "stats come from the filesystem")
        compare(ctx.props.visible, true)
        compare(ctx.props.fileName, "a.mp4")
        compare(ctx.props.permissions, "rw-r--r--")
        compare(ctx.props.owner, "tester")
        compare(ctx.props.mimeType, "video/mp4")
        compare(ctx.props.size.length > 0, true)
        compare(ctx.props.favorite, "Yes")
        compare(ctx.props.watched, "No")
    }

    // --- Delete confirmation is permanent-delete only (D-31) ----------------
    function test_permanentDeleteConfirmsWhileTrashDoesNot() {
        const ctx = makeContext(1)
        ctx.content.trashRequested(0)
        compare(ctx.confirm.visible, false, "Trash is undoable → no confirmation")
        compare(ctx.actions.trashCalls.length, 1)
        compare(ctx.actions.trashCalls[0], "/v/a.mp4")

        ctx.content.permanentDeleteRequested(0)
        compare(ctx.confirm.visible, true)
        compare(ctx.confirm.firstItemName, "a.mp4")
        compare(ctx.actions.trashCalls.length, 1, "nothing destroyed yet")

        ctx.confirm.confirmed()
        compare(ctx.confirm.visible, false)
        // Confirming must actually dispatch the permanent delete — not a no-op
        // and not a placeholder "not available" toast (D-31 / REM-14).
        compare(ctx.actions.deleteCalls.length, 1, "confirm dispatches deletePermanently")
        compare(ctx.actions.deleteCalls[0].length, 1)
        compare(ctx.actions.deleteCalls[0][0], "/v/a.mp4")
    }

    // --- Toast feedback + Undo (D-08) ---------------------------------------
    function test_operationToastCarriesUndoAndUndoReachesController() {
        const ctx = makeContext(0)
        ctx.actions.toastRequested("Moved to Trash", "trash", "/home/tester/.local/share/Trash/x")

        verify(ctx.toast.message.indexOf("Trash") !== -1)
        compare(ctx.toast.undoKind, "trash")
        compare(ctx.toast.hasUndo, true)

        ctx.toast.undoTapped()
        compare(ctx.actions.undoLastTrashCalls.length, 1)
    }

    function test_failureToastFromControllerIsVisible() {
        const ctx = makeContext(0)
        compare(ctx.toast.message.length, 0)
        ctx.actions.actionFinished("rename", false)
        verify(ctx.toast.message.indexOf("rename") !== -1)
        // Success never announces itself.
        const before = ctx.toast.message
        ctx.actions.actionFinished("rename", true)
        compare(ctx.toast.message, before)
    }

    // --- Content-state overlay + precedence (§12) ---------------------------
    function test_overlayShowsEmptyAndIsSuppressedWithContent() {
        const empty = makeContext(0)
        compare(empty.shell.contentState, "empty")
        compare(empty.overlay.visible, true)
        compare(empty.overlay.appliedState, "empty")
        verify(empty.overlay.title.length > 0)

        const filled = makeContext(3)
        compare(filled.shell.contentState, "")
        compare(filled.overlay.visible, false)
    }

    function test_overlayLoadingOnlyWhileNothingIsShown() {
        const ctx = makeContext(0)
        ctx.browser.busy = true
        compare(ctx.shell.contentState, "loading")
        compare(ctx.overlay.visible, true)
        compare(ctx.overlay.appliedState, "loading")
        compare(ctx.overlay.title.length, 0, "loading keeps a bare skeleton")

        // Streaming into a populated grid must not cover the content.
        ctx.folder.count = 5
        compare(ctx.shell.contentState, "")
        compare(ctx.overlay.visible, false)
    }

    function test_overlayAccessDeniedAndErrorAreDistinct() {
        const ctx = makeContext(0)
        ctx.browser.errorOccurred("/videos", "Permission denied")
        compare(ctx.shell.contentState, "accessDenied")
        compare(ctx.overlay.visible, true)
        verify(ctx.overlay.title.indexOf("permission") !== -1)

        // Recovery: navigating clears the failure.
        ctx.browser.currentPathChanged()
        compare(ctx.shell.contentState, "empty")

        ctx.browser.errorOccurred("/videos", "boom")
        compare(ctx.shell.contentState, "error")
        verify(ctx.overlay.title.indexOf("wrong") !== -1)
    }

    function test_overlayDisconnectedWinsOverEverything() {
        const ctx = makeContext(0)
        ctx.mounts.disconnected = true
        ctx.browser.currentPath = "/media/usb/clip.mp4"
        // An access failure inside a vanished mount is still a disconnect first.
        ctx.browser.errorOccurred("/media/usb/clip.mp4", "Permission denied")

        compare(ctx.shell.contentState, "disconnected")
        compare(ctx.overlay.appliedState, "disconnected")
        compare(ctx.overlay.driveName, "usb stick")
        verify(ctx.overlay.title.indexOf("usb") !== -1, "names the volume")

        // Once the volume is back, the previous failure state applies again.
        ctx.mounts.disconnected = false
        compare(ctx.shell.contentState, "accessDenied")
    }

    function test_overlayActionsMoveTheUserForward() {
        // Error → retry re-enumerates.
        const errored = makeContext(0)
        errored.browser.errorOccurred("/videos", "boom")
        errored.overlay.actionTriggered()
        compare(errored.browser.refreshes, 1)

        // Access denied → copy the path (the only recovery QG can offer here).
        const denied = makeContext(0)
        denied.browser.errorOccurred("/videos", "Permission denied")
        denied.overlay.actionTriggered()
        compare(denied.actions.copyPathCalls.length, 1)
        compare(denied.actions.copyPathCalls[0], "/videos")

        // Empty → go home.
        const empty = makeContext(0)
        empty.overlay.actionTriggered()
        compare(empty.browser.navigations.length, 1)
        compare(empty.browser.navigations[0], "/home/tester")
    }
}
