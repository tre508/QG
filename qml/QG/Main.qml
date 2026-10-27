import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
// `Window.FullScreen` / `Window.Windowed`: the Cinema window-state transition
// (playback §4.2/§4.5) reads and writes `visibility`, whose enum lives here.
import QtQuick.Window

import qg

// QG application shell (DESIGN.md §3.1–§3.6).
//
// The shell composes the six window regions. Its C++ dependencies
// (browserController/folderModel/placesModel/mounts) are injected by the
// application after loading (see src/app.cpp); they default to null so the
// shell also loads in tests without an application. Presentation only — all
// intent is forwarded to the injected controller/service.
//
// Region object names are a test/automation contract:
//   sidebarRegion, pathBarRegion, contextStripRegion,
//   contentSurfaceRegion, reelRegion, statusStripRegion.

ApplicationWindow {
    id: window

    readonly property real contentWidth: width

    property var browserController: null
    property var folderModel: null
    property var placesModel: null
    property var mounts: null
    property var playbackController: null
    property var playbackContext: null
    property var playbackManager: null
    property var reelModel: null
    // Remaining subsystems composed by App (review.md D-01).
    property var stateController: null
    property var thumbnailController: null
    property var probeController: null
    property var searchProxy: null
    property var searchController: null
    property var actionsController: null
    property var mountsController: null
    property var selectionState: null
    // --- Selection accessors (AUD-004 / FIX-SEL-01) --------------------------
    // SelectionState exposes selection/focus as *properties* so that a binding
    // reading them re-evaluates when the selection changes; the previous
    // invokable-only reads (`selectedCount()`) created no dependency, which is
    // why the status strip and the context-menu title froze at their
    // creation-time values. Reading the property here is what creates that
    // dependency; an invokable-shaped double is still accepted.
    function selectionCount() {
        const s = window.selectionState
        if (!s)
            return 0
        const v = s.selectedCount
        return typeof v === "function" ? v() : ((v === undefined || v === null) ? 0 : v)
    }
    function selectionIds() {
        const s = window.selectionState
        if (!s)
            return []
        const v = s.selectedIdentities
        if (typeof v === "function")
            return v()
        return (v === undefined || v === null) ? [] : v
    }
    // --- Search accessors (AUD-005 / FIX-SRCH-01) ---------------------------
    // SearchController now owns `active`/`summary`/`resultCount` as *properties*
    // (DESIGN.md §7.2). These read them, so the value used by the surface switch
    // is the controller's, and still accept an invokable-shaped test double
    // (the in-repo QML fixture declares `function summary()`).
    function searchIsActive(fallback) {
        const c = window.searchController
        if (!c)
            return fallback === true
        const v = c.active
        if (v === undefined || v === null)
            return fallback === true
        return v === true
    }
    function searchSummaryText(fallback) {
        const c = window.searchController
        if (c) {
            const v = c.summary
            if (typeof v === "string")
                return v
        }
        return (fallback === undefined || fallback === null) ? "" : fallback
    }
    // The applied (debounced) query, owned by the controller.
    function searchAppliedQuery() {
        const c = window.searchController
        if (!c || typeof c.query !== "function")
            return ""
        return c.query()
    }
    // Central keyboard router (REMEDIATION-012 / D-11). Null in tests that do not
    // inject it; every shortcut no-ops safely when absent.
    property var keyboard: null
    // Places sidebar visibility — F9 toggles it (DESIGN.md §13.2).
    property bool placesVisible: true

    // Standard-density card target per the DESIGN.md §3.5 responsive table.
    // The shell owns the window-width bands; ContentView owns the geometry math.
    readonly property int gridTargetCardWidth:
        window.width >= 1500 ? 240 :
        window.width >= 1150 ? 220 :
        window.width >= 900 ? 200 : 190

    // Sidebar responsive geometry (DESIGN.md §3.3, §3.5).
    // Panel (240) at >= 1500px; rail (56) at 820-1499px; hidden (< 820px, overlay).
    readonly property int sidebarPanelWidth: Theme.scale(240)
    readonly property int sidebarRailWidth: Theme.scale(56)
    readonly property bool sidebarInRailMode: window.width >= 820 && window.width < 1500
    readonly property bool sidebarVisible: window.width >= 820 && !window.cinema && window.placesVisible
    readonly property real sidebarLayoutWidth: sidebarVisible ? (sidebarInRailMode ? sidebarRailWidth : sidebarPanelWidth) : 0

    // Reel visibility: auto-opens with playback, toggled with Ctrl+R, closed
    // with its ✕ (DESIGN.md §10.2).
    property bool reelVisible: false

    // Window state to restore when Cinema exits (DESIGN.md playback §4.2/§4.5:
    // fullscreen uses the existing window; exiting returns to the prior state).
    property int windowedVisibility: Window.Windowed

    // --- Dialogs / feedback state (REMEDIATION-010) --------------------------
    property string pendingRenamePath: ""
    property string pendingRenameBase: ""
    property string pendingRenameTargetPath: ""
    property int pendingRenameIndex: -1
    property string pendingRenameIdentity: ""
    // Permanent-delete captures paths AND identities at confirm time (identity
    // must be resolved before the file vanishes — inodes can be reused).
    property var pendingPermanentPaths: []
    property var pendingPermanentIdentities: []
    property int pendingTrashIndex: -1
    property string pendingTrashIdentity: ""
    // Last browser enumeration/access failure, for the content-state overlay.
    property string browserLastError: ""
    property bool browserAccessDenied: false
    readonly property bool browserBusy: window.browserController
                                        ? window.browserController.busy === true : false

    // DESIGN.md §12 precedence: disconnected > access denied > error > loading
    // (only while nothing is shown) > empty. A folder-only directory is NOT an
    // empty state — its folders render — so it never reaches the overlay.
    readonly property string contentState: {
        if (window.cinema || window.manifestActive)
            return ""
        const path = window.browserController ? window.browserController.currentPath : ""
        if (window.mountsController && window.mountsController.disconnected === true
                && window.mountsController.isInsideDisconnectedVolume(path) === true)
            return "disconnected"
        if (window.browserLastError.length > 0)
            return window.browserAccessDenied ? "accessDenied" : "error"
        const count = (window.folderModel && window.folderModel.count !== undefined)
                      ? window.folderModel.count : 0
        if (window.browserBusy && count === 0)
            return "loading"
        if (window.browserController && count === 0 && !window.browserBusy)
            return "empty"
        return ""
    }
    readonly property bool manifestActive: window.searchController
                                           && window.searchController.active === true

    // Cinema layout state (DESIGN.md §9): chrome regions collapse.
    readonly property bool cinema: window.playbackController
                                   && window.playbackController.cinemaActive === true

    // The live playback session, if any (Stage/Cinema read this).
    readonly property var activeSession: window.playbackController
                                         ? window.playbackController.currentSession : null
    //     // PlaybackSession::Idle is 0 in playback_session.hpp.
    // Reject absent sessions and unavailable state properties.
    readonly property bool sessionActive: window.activeSession != null
                                          && typeof window.activeSession.state === "number"
                                          && window.activeSession.state !== 0
    // True while a text editor (rename field, path entry, search field) holds
    // focus. Transport/Cinema shortcuts are gated on this so Esc never shadows
    // the editor's own handling (REMEDIATION-006 task 3).
    readonly property bool textEditing: {
        const item = window.activeFocusItem
        if (!item)
            return false
        const kind = "" + item
        return kind.indexOf("QQuickTextInput") !== -1
                || kind.indexOf("QQuickTextEdit") !== -1
                || item.echoMode !== undefined
    }

    // Playback shortcuts are live while the Stage holds focus (or while Cinema,
    // which focuses the Stage, is active) — DESIGN.md §13.2: Space/K/J/L/… apply
    // "Stage focused", while the same keys mean selection/movement while browsing.
    readonly property bool stageKeys: window.sessionActive && !window.textEditing
                                      && (window.cinema || stage.activeFocus)

    // Single dispatch point for every window-scoped shortcut. The KeyboardFramework
    // owns the routing decision; this only forwards the action id.
    function routeKey(id) {
        if (window.keyboard && window.keyboard.dispatch)
            window.keyboard.dispatch(id)
    }

    function toggleCinema() {
        if (window.playbackController)
            window.playbackController.toggleCinema()
    }

    // Esc: in Cinema → return to Stage; in Stage → un-bloom to the grid.
    function escapePlayback() {
        if (!window.playbackController)
            return
        if (window.cinema)
            window.playbackController.exitCinema()
        else if (window.sessionActive)
            window.playbackController.stop()
    }

    // Focused-row state toggles, reached from the central keyboard map.
    function toggleFavoriteAt(index) {
        if (!window.stateController || index < 0)
            return
        const id = contentView.identityForIndex(index)
        if (id.length > 0)
            window.stateController.toggleFavorite(id)
    }

    function toggleWatchedAt(index) {
        if (!window.stateController || index < 0)
            return
        const id = contentView.identityForIndex(index)
        if (id.length > 0)
            window.stateController.toggleWatched(id)
    }

    // --- Card actions → controllers (REMEDIATION-009 / D-04) ----------------
    function playAt(index) {
        if (!window.playbackController || index < 0)
            return
        const path = contentView.pathForIndex(index)
        if (path.length === 0)
            return
        window.playbackController.play(path, contentView.identityForIndex(index))
    }

    // The card overflow control opens the context menu (never a no-op). The item
    // list comes from ActionsController; dispatch goes through a named function
    // so it is invocable from a test (Quick Test cannot trigger a MenuItem).
    function openContextMenu(index, globalX, globalY) {
        contentMenu.targetIndex = index
        window.rebuildContextMenu(window.menuSelectionCount)
        contentMenu.popup(globalX === undefined ? 0 : globalX,
                          globalY === undefined ? 0 : globalY)
    }

    // Multi-selection state for the menu title + bulk-only verbs (DESIGN §11.2).
    readonly property int menuSelectionCount: {
        const n = window.selectionCount()
        if (n > 0)
            return n
        // The menu always acts on at least the focused row (§11.2).
        return 1
    }

    // --- Dialogs (DESIGN.md §11.1) ------------------------------------------
    function openProperties(index) {
        if (!window.actionsController || index < 0)
            return
        const path = contentView.pathForIndex(index)
        if (path.length === 0)
            return
        const info = window.actionsController.fileInfo(path)
        propertiesDialog.filePath = info.path !== undefined ? info.path : path
        propertiesDialog.fileName = info.name !== undefined ? info.name : ""
        propertiesDialog.size = info.exists ? Format.size(info.sizeBytes) : qsTr("Unknown")
        propertiesDialog.modified = info.exists ? Format.date(info.mtimeNs) : ""
        propertiesDialog.permissions = info.permissions !== undefined ? info.permissions : ""
        propertiesDialog.owner = info.owner !== undefined ? info.owner : ""
        propertiesDialog.mimeType = info.mimeType !== undefined ? info.mimeType : ""
        // QG metadata comes from the canonical model row (identity-keyed state).
        propertiesDialog.favorite = window.modelBoolAt(index, "favorite") ? qsTr("Yes") : qsTr("No")
        propertiesDialog.watched = window.modelBoolAt(index, "watched") ? qsTr("Yes") : qsTr("No")
        propertiesDialog.visible = true
    }

    // Reads a boolean model helper accessor (folderModel.favoriteAt/watchedAt).
    function modelBoolAt(index, kind) {
        if (!window.folderModel || index < 0)
            return false
        const fn = kind === "favorite" ? window.folderModel.favoriteAt
                                       : window.folderModel.watchedAt
        return typeof fn === "function" ? fn.call(window.folderModel, index) === true : false
    }

// Rename: collide → the conflict dialog; otherwise rename straight away.
    function requestRename(index, newBaseName) {
        if (!window.actionsController || index < 0 || newBaseName.length === 0)
            return
        const path = contentView.pathForIndex(index)
        if (path.length === 0)
            return
        const identity = contentView.identityForIndex(index)
                const target = window.actionsController.targetPathForRename(path, newBaseName)
        window.pendingRenameTargetPath = target

        if (target !== path && window.actionsController.pathExists(target)) {
            window.pendingRenamePath = path
            window.pendingRenameBase = newBaseName
            window.pendingRenameIndex = index
            window.pendingRenameIdentity = identity
            renameConflictDialog.targetName = target.split("/").pop()
            renameConflictDialog.visible = true
            return
        }
        window.pendingRenamePath = path
        window.pendingRenameBase = newBaseName
        window.pendingRenameIndex = index
        window.pendingRenameIdentity = identity
        window.actionsController.rename(path, newBaseName)
    }

    function applyKeepBothRename() {
        if (!window.actionsController || window.pendingRenamePath.length === 0)
            return
                const unique = window.actionsController.uniqueBaseName(window.pendingRenamePath,
                                                                window.pendingRenameBase)

        window.pendingRenameTargetPath =
            window.actionsController.targetPathForRename(
                window.pendingRenamePath, unique)

        window.actionsController.rename(window.pendingRenamePath, unique)
        // pendingRenameIndex/Identity already set from the initial requestRename
    }

        function clearPendingRename() {
        window.pendingRenamePath = ""
        window.pendingRenameBase = ""
        window.pendingRenameTargetPath = ""
        window.pendingRenameIndex = -1
        window.pendingRenameIdentity = ""
    }

    // The overlay's one primary action moves the user forward (DESIGN §12:
    // "errors are recoverable, not terminal").
    function contentStateAction() {
        const state = window.contentState
        if (state === "disconnected" || state === "error") {
            if (window.browserController) window.browserController.refresh()
        } else if (state === "accessDenied") {
            if (window.actionsController && window.browserController)
                window.actionsController.copyPath(window.browserController.currentPath)
        } else if (state === "empty") {
            if (window.browserController && window.browserController.homePath !== undefined)
                window.browserController.navigate(window.browserController.homePath())
        }
    }

    function trashAt(index) {
        if (!window.actionsController || index < 0)
            return
        const path = contentView.pathForIndex(index)
        const identity = contentView.identityForIndex(index)
        if (path.length > 0) {
            window.pendingTrashIndex = index
            window.pendingTrashIdentity = identity
            window.actionsController.trashOne(path)
        }
    }

    // Permanent deletion is the ONLY path that confirms (DESIGN.md §11.1).
    function requestPermanentDelete(index) {
        let paths = []
        let identities = []
        // Multi-selection: delete the full selected set (bulk verb, §11.4).
        if (window.selectionCount() > 1 && window.folderModel) {
            const ids = window.selectionIds()
            const total = window.folderModel.count
            for (let r = 0; r < total; ++r) {
                const id = window.folderModel.identityAt(r)
                if (id.length > 0 && ids.indexOf(id) >= 0) {
                    paths.push(window.folderModel.pathAt(r))
                    identities.push(id)
                }
            }
        } else if (index >= 0) {
            const path = contentView.pathForIndex(index)
            const identity = contentView.identityForIndex(index)
            if (path.length > 0) {
                paths = [path]
                identities = [identity.length > 0 ? identity : ""]
            }
        }
        if (paths.length === 0)
            return
        window.pendingPermanentPaths = paths
        window.pendingPermanentIdentities = identities
        confirmDeleteDialog.itemCount = paths.length
        confirmDeleteDialog.firstItemName = paths[0].split("/").pop()
        confirmDeleteDialog.visible = true
    }

    function dispatchMenuAction(label) {
        const index = contentMenu.targetIndex
        if (index < 0)
            return
        const path = contentView.pathForIndex(index)
        const identity = contentView.identityForIndex(index)
        switch (label) {
        case "Play":
            window.playAt(index)
            break
        case "Favorite":
            if (window.stateController && identity.length > 0)
                window.stateController.toggleFavorite(identity)
            break
        case "Mark Watched":
            if (window.stateController && identity.length > 0)
                window.stateController.toggleWatched(identity)
            break
        case "Rename":
            contentView.beginRename(index)
            break
        case "Reveal":
            if (window.actionsController) window.actionsController.reveal(path)
            break
        case "Open With":
            if (window.actionsController) window.actionsController.openWith(path)
            break
        case "Properties":
            // Opens QG's Properties surface (DESIGN.md §11.1), populated from
            // real filesystem stats — never a fabricated row.
            window.openProperties(index)
            break
        case "Copy":
            if (window.actionsController) window.actionsController.copyToClipboard([path])
            break
        case "Cut":
            if (window.actionsController) window.actionsController.cut([path])
            break
        case "Move to Trash":
            // Trash is undoable, so it does not confirm (DESIGN.md §11.1).
            window.trashAt(index)
            break
        case "Delete Permanently":
            // High risk: always confirmed.
            window.requestPermanentDelete(index)
            break
        case "Add to Collection (coming soon)": // disabled in the menu
        default:
            break
        }
    }

    width: 1280
    height: 800
    minimumWidth: 480
    minimumHeight: 360
    visible: true
    title: qsTr("QG")
    color: Theme.ink

    component RegionLabel: Text {
        anchors.fill: parent
        anchors.margins: Theme.space3
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        color: Theme.textLow
        font.pixelSize: Theme.fontMeta
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0


        // Places sidebar · Workspace · Reel --------------------------------
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Rectangle {
                objectName: "sidebarRegion"
                Layout.preferredWidth: sidebarLayoutWidth
                Layout.fillHeight: true
                visible: sidebarVisible
                color: Theme.surface1

                PlacesSidebar {
                    id: sidebar
                    anchors.fill: parent
                    model: window.placesModel
                    onLocationActivated: function (path) {
                        if (window.browserController) {
                            window.browserController.navigate(path)
                        }
                    }
                    onVolumeEjectRequested: function (volumeId) {
                        if (window.mounts) {
                            window.mounts.eject(volumeId)
                        }
                    }
                    onVolumeUnmountRequested: function (volumeId) {
                        if (window.mounts) {
                            window.mounts.unmount(volumeId)
                        }
                    }
                    onPinRequested: function (path) {
                        if (window.placesModel) {
                            window.placesModel.requestPin(path)
                        }
                    }
                    onUnpinRequested: function (path) {
                        if (window.placesModel) {
                            window.placesModel.requestUnpin(path)
                        }
                    }
                    onFileDropped: function (filePath, targetDir, isMove) {
                        if (window.actionsController && filePath.length > 0) {
                            const paths = [filePath]
                            const dest = targetDir || (window.browserController ? (window.browserController.currentPath || "") : "")
                            if (isMove) {
                                window.actionsController.move(paths, dest)
                            } else {
                                window.actionsController.copy(paths, dest)
                            }
                        }
                    }
                }
            }

            ColumnLayout {
                objectName: "workspaceRegion"
                Layout.fillWidth: true
                Layout.fillHeight: true
                spacing: 0

                // Path Bar ---------------------------------------------------
                Rectangle {
                    objectName: "pathBarRegion"
                    Layout.fillWidth: true
                    implicitHeight: window.cinema ? 0 : 48
                    visible: !window.cinema
                    color: Theme.surface1

                    PathBar {
                        id: pathBar
                        anchors.fill: parent
                        controller: window.browserController
                        // Live count from the search proxy (D-03).
                        searchResultCount: (window.searchProxy
                                            && window.searchProxy.resultCount !== undefined)
                                           ? window.searchProxy.resultCount : 0
                        onOrderChanged: function (key, descending, foldersFirst) {
                            if (window.browserController
                                    && window.browserController.setSort !== undefined)
                                window.browserController.setSort(key, descending, foldersFirst)
                        }
                        // Morph field text → SearchController (debounced there).
                        onSearchQueryChanged: function (query) {
                            if (window.searchController)
                                window.searchController.setQuery(query)
                        }
                        onScopeToggled: function (subfolders) {
                            if (window.searchController) {
                                window.searchController.setScope(subfolders)
                            }
                        }
                        onDensitySelected: function (d) {
                            if (window.browserController
                                    && window.browserController.setDensityValue !== undefined)
                                window.browserController.setDensityValue(d)
                        }
                        onShowHiddenToggled: function (show) {
                            if (window.browserController
                                    && window.browserController.setShowHidden !== undefined)
                                window.browserController.setShowHidden(show)
                        }
                    }
                }

                // Context strip ----------------------------------------------
                Rectangle {
                    objectName: "contextStripRegion"
                    Layout.fillWidth: true
                    implicitHeight: contextStrip.visible ? 28 : 0
                    visible: !window.cinema
                    color: Theme.surface0
                    clip: true

                    ContextStrip {
                        id: contextStrip
                        anchors.fill: parent
                        folderName: (window.browserController
                                     && window.browserController.folderName !== undefined)
                                    ? window.browserController.folderName : ""
                        videoCount: (window.folderModel && window.folderModel.videoCount !== undefined)
                                    ? window.folderModel.videoCount : 0
                        folderCount: (window.folderModel && window.folderModel.folderCount !== undefined)
                                    ? window.folderModel.folderCount : 0
                        orderLabel: (window.browserController
                                    && window.browserController.orderLabel !== undefined)
                                   ? window.browserController.orderLabel : "Name \\u2191"
                        density: (window.browserController
                                  && window.browserController.density !== undefined)
                                 ? window.browserController.density : contentView.density
                        onDensitySelected: function (d) {
                            if (window.browserController
                                    && window.browserController.setDensityValue !== undefined)
                                window.browserController.setDensityValue(d)
                        }
                    }
                }

                // Content surface --------------------------------------------
                Rectangle {
                    objectName: "contentSurfaceRegion"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    color: Theme.surface0

ContentView {
                    id: contentView
                    anchors.fill: parent
                    // Reflow, don't overlay: the grid starts below the Stage
                    // (DESIGN.md §3.4, playback §2.1.7, §2.2; AUD-009).
                    reservedTop: (window.sessionActive && !window.cinema && stage.visible)
                                 ? stage.height + Theme.space3 : 0
                    model: window.folderModel
                    recursiveModel: window.searchController ? window.searchController.recursiveResultsModel() : null
                    searchRecursive: window.searchController ? window.searchController.scopeRecursive() : false
                    selectionState: window.selectionState
                    typeToSelectBuffer: window.typeToSelectBuffer
                    targetCardWidth: window.gridTargetCardWidth
                    density: (window.browserController
                              && window.browserController.density !== undefined)
                           ? window.browserController.density : ContentView.Standard
                    onActivatedItem: function (index, path, isFolder) {
                        if (isFolder && window.browserController) {
                            window.browserController.navigate(path)
                        } else if (!isFolder && window.playbackController
                                   && window.folderModel) {
                            // Play inline (DESIGN.md §15.2 canonical play flow):
                            // double-click/Enter activates; selection alone does not play.
                            const identity = (window.folderModel.identityAt !== undefined)
                                            ? window.folderModel.identityAt(index) : ""
                            window.playbackController.play(path, identity)
                        }
                    }

                    // --- Card actions → composed controllers (REM-009 / D-04) ---
                    onPlayRequested: function (index) {
                        window.playAt(index)
                    }
                    onFavoriteToggled: function (index) {
                        // State is identity-keyed and persists immediately; the
                        // model row updates so the badge shows without a refresh.
                        if (window.stateController) {
                            const id = contentView.identityForIndex(index)
                            if (id.length > 0)
                                window.stateController.toggleFavorite(id)
                        }
                    }
                    onWatchedToggled: function (index) {
                        if (window.stateController) {
                            const id = contentView.identityForIndex(index)
                            if (id.length > 0)
                                window.stateController.toggleWatched(id)
                        }
                    }
                    onRenameCommitted: function (index, newName) {
                        // A collision opens RenameConflictDialog; otherwise the
                        // rename goes straight to the controller (§11.3).
                        window.requestRename(index, newName)
                    }
                    onTrashRequested: function (index) {
                        // Delete → Trash (undoable, no confirmation).
                        window.trashAt(index)
                    }
                    onPermanentDeleteRequested: function (index) {
                        // Shift+Delete → confirm dialog (§11.1).
                        window.requestPermanentDelete(index)
                    }
                    onThumbnailRetryRequested: function (index) {
                        if (window.thumbnailController) {
                            const id = contentView.identityForIndex(index)
                            if (id.length > 0)
                                window.thumbnailController.retry(id)
                        }
                    }
                    onOverflowRequested: function (index, globalX, globalY) {
                        window.openContextMenu(index, globalX, globalY)
                    }
                    // §7.4 / §25.4: the no-results "Clear search" is the same intent
                    // as the field's ✕ — it clears the query, and the controller's
                    // notification restores Browse. Routing it through the field
                    // keeps SearchController the owner of the query lifecycle;
                    // dropping the Manifest inside the view left the morph open and
                    // the controller still active over a Browse grid.
                    onSearchClearRequested: pathBar.clearSearchQuery()
                    onDropRequested: function (paths, targetDir, isMove) {
                        if (window.actionsController && paths && paths.length > 0) {
                            const dest = targetDir || (window.browserController && window.browserController.currentPath ? window.browserController.currentPath : "")
                            if (isMove) {
                                window.actionsController.move(paths, dest)
                            } else {
                                window.actionsController.copy(paths, dest)
                            }
                        }
                    }
                    onSortRequested: function (key, descending, foldersFirst) {
                        if (window.browserController && window.browserController.setSort) {
                            window.browserController.setSort(key, descending, foldersFirst)
                        }
                    }

                    // Viewport feed (REM-01): the visible canonical-model rows
                    // drive the thumbnail and probe schedulers, so only visible
                    // items trigger decode/probe work (DESIGN.md §18.3, §22.4).
                    onVisibleRangeChanged: function (first, last) {
                        if (window.thumbnailController)
                            window.thumbnailController.requestVisible(first, last)
                        if (window.probeController)
                            window.probeController.requestVisible(first, last)
                    }
                }

                // Content-state overlay (DESIGN.md §12): empty / access denied /
                // disconnected / error. It sits *under* the Stage so playback
                // always wins, and never covers a populated grid (§12: a
                // folder-only directory renders its folders, no empty state).
                StateOverlay {
                    id: contentStateOverlay
                    objectName: "contentStateOverlay"
                    visible: window.contentState.length > 0 && !window.sessionActive
                    // A disconnected drive is named in the copy (DESIGN.md §12).
                    driveName: window.mountsController
                               && window.mountsController.disconnectedDrive !== undefined
                               ? window.mountsController.disconnectedDrive : ""
                    // Reactive copy: the overlay follows the computed state
                    // (DESIGN §12 wording, never application-controlled).
                    property string appliedState: window.contentState
                    onAppliedStateChanged: applyState(appliedState, "")
                    Component.onCompleted: applyState(window.contentState, "")
                    onActionTriggered: window.contentStateAction()
                }

                // Indeterminate loading bar (DESIGN.md §12: a thin bar under the
                // Path Bar while enumeration runs; loading never blocks).
                Rectangle {
                    id: loadingBar
                    objectName: "loadingBar"
                    anchors.top: parent.top
                    anchors.left: parent.left
                    anchors.right: parent.right
                    height: 2
                    color: Theme.surface4
                    visible: window.browserBusy

                    Rectangle {
                        id: loadingPulse
                        height: parent.height
                        width: parent.width * 0.25
                        color: Theme.signal

                        // `parent` inside the animation is the animation, not the
                        // track — address the track by id.
                        SequentialAnimation on x {
                            running: window.browserBusy && Theme.motionEnabled
                            loops: Animation.Infinite
                            NumberAnimation { from: 0; to: Math.max(0, loadingBar.width - loadingPulse.width); duration: 900; easing.type: Easing.InOutSine }
                            NumberAnimation { from: Math.max(0, loadingBar.width - loadingPulse.width); to: 0; duration: 900; easing.type: Easing.InOutSine }
                        }
                    }
                }

                // Stage overlay: inline playback surface (DESIGN.md §8.1, §9).
                // Visible when a playback session is active; in Cinema it fills
                // the whole window (P54), otherwise it spans content width.
                Stage {
                    id: stage
                    objectName: "stageRegion"
                    // Inline Stage: a real element of the content flow (DESIGN.md
                    // §3.4). ContentView reserves its height (`reservedTop`) so the
                    // cards reflow beneath it rather than being covered; in Cinema
                    // the Stage fills the window and the inset is irrelevant.
                    // Anchored top/left so the Stage computes its own width and
                    // height (inline 60% cap vs. Cinema full-window) from the
                    // region — never from the inset, so no binding loop exists.
                    anchors.top: parent.top
                    anchors.left: parent.left
                    visible: window.sessionActive
                    // Cinema fills the whole window (DESIGN.md §9.1).
                    cinema: window.cinema
                    backend: window.playbackManager ? window.playbackManager.backend : null
                    playbackManager: window.playbackManager
                    playbackController: window.playbackController
                    session: window.activeSession
                    playing: window.playbackController
                             && window.playbackController.playing === true
                    currentPath: window.activeSession ? window.activeSession.path : ""
                    currentName: (window.activeSession && window.activeSession.path)
                                 ? window.activeSession.path.split("/").pop() : ""
                    errorText: window.activeSession ? window.activeSession.errorMessage : ""
                    currentNumber: (window.playbackContext
                                    && window.playbackContext.currentIndex >= 0)
                                   ? window.playbackContext.currentIndex + 1 : 0
                    totalCount: window.playbackContext ? window.playbackContext.totalCount : 0
                    onCloseRequested: {
                        // ✕ un-blooms to the grid and always leaves Cinema
                        // (PlaybackController::stop handles both — §9.3).
                        if (window.playbackController) window.playbackController.stop()
                    }
                    onFullscreenRequested: window.toggleCinema()
                }

                // (Cinema/Stage/fullscreen shortcuts live in the central keyboard
                // map at the bottom of this file — REMEDIATION-012 / D-11.)

                // Toast host: bottom-center of the content surface (P64).
                ToastHost {
                    id: toastHost
                    anchors.bottom: parent.bottom
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.bottomMargin: Theme.space4

                    // Undo lives in the controller (it holds the trash record);
                    // the toast only carries the affordance (D-08).
                    onUndoTapped: {
                        if (window.actionsController && window.actionsController.undoLastTrash()) {
                            // Refresh the folder to pick up the restored file
                            if (window.browserController) window.browserController.refresh()
                        }
                        toastHost.message = ""
                    }
                }
            }
            }

            Rectangle {
                objectName: "reelRegion"
                Layout.preferredWidth: 280
                Layout.fillHeight: true
                // Opens with playback; toggled with Ctrl+R; ✕ closes (§10.2).
                visible: !window.cinema && reelModel !== null && window.reelVisible
                color: Theme.surface1

                ReelPanel {
                    id: reelPanel
                    anchors.fill: parent
                    reelModel: window.reelModel
                    folderName: (window.browserController
                                 && window.browserController.folderName !== undefined)
                                ? window.browserController.folderName : ""
                    sortLabel: (window.browserController
                                && window.browserController.orderLabel !== undefined)
                               ? window.browserController.orderLabel : ""
                    positionLabel: (window.playbackController
                                    && window.playbackController.positionLabel !== undefined)
                                   ? window.playbackController.positionLabel : ""
                    filterLabel: window.playbackController
                                 ? ["All", "Favorites", "Unwatched"][window.playbackController.reelFilter]
                                 : "All"
                    canGoPrevious: window.playbackController
                                   && window.playbackController.canGoPrevious === true
                    canGoNext: window.playbackController
                               && window.playbackController.canGoNext === true
                    onPreviousRequested: {
                        if (window.playbackController) window.playbackController.previous()
                    }
                    onNextRequested: {
                        if (window.playbackController) window.playbackController.next()
                    }
                    onFavoriteToggled: {
                        // Footer favorite acts on the current playback item by
                        // identity (state is identity-keyed, DESIGN.md §21.3).
                        if (window.stateController && window.playbackController) {
                            const id = window.playbackController.currentIdentity()
                            if (id.length > 0)
                                window.stateController.toggleFavorite(id)
                        }
                    }
                    onFilterChanged: function (f) {
                        if (!window.playbackController) return
                        const idx = ["All", "Favorites", "Unwatched"].indexOf(f)
                        window.playbackController.setReelFilter(Math.max(0, idx))
                    }
                    onItemActivated: function (row) {
                        if (window.playbackController)
                            window.playbackController.selectReelRow(row)
                    }
                    onCloseRequested: window.reelVisible = false
                }
            }
        }

        // Status strip -------------------------------------------------------
        Rectangle {
            objectName: "statusStripRegion"
            Layout.fillWidth: true
            // Cinema hides the whole surrounding chrome (DESIGN.md §9.1).
            implicitHeight: window.cinema ? 0 : 28
            visible: !window.cinema
            color: Theme.surface1

            StatusStrip {
                id: statusStrip
                anchors.fill: parent
                // Multi-selection is now tracked via SelectionState (REM-013 / D-12, D-29).
                selectedCount: window.selectionCount()
                selectedBytes: {
                    if (!window.selectionState || !window.folderModel)
                        return 0
                    let total = 0
                    const ids = window.selectionIds()
                    for (let i = 0; i < ids.length; ++i) {
                        let idx = -1
                        if (typeof window.folderModel.indexOfIdentity === "function") {
                            idx = window.folderModel.indexOfIdentity(ids[i])
                        }
                        if (idx >= 0 && typeof window.folderModel.sizeAt === "function")
                            total += window.folderModel.sizeAt(idx)
                    }
                    return total
                }
                // Free space of the current volume comes from the controller's
                // statvfs figure (0/unknown renders no claim: DESIGN.md §12).
                volumeFreeBytes: window.browserController
                                 ? (window.browserController.freeSpaceBytes > 0
                                    ? window.browserController.freeSpaceBytes : -1)
                                 : -1
                busyCount: (window.browserController && window.browserController.busy) ? 1 : 0
            }
        }
    }

    // Search pipeline (review.md D-03): the SearchController drives the
    // Manifest presentation; the PathBar field drives the controller.
    Connections {
        target: window.searchController

        function onActiveChanged(active) {
            if (window.searchIsActive(active)) {
                // Give the summary its folder/sort context before it is emitted.
                if (window.browserController) {
                    const folder = (window.browserController.folderName !== undefined)
                                 ? window.browserController.folderName : ""
                    const order = (window.browserController.orderLabel !== undefined)
                                ? window.browserController.orderLabel : ""
                    window.searchController.setFolderName(folder)
                    window.searchController.setSortLabel(order)
                }
                contentView.enterManifest(window.searchProxy,
                                          window.searchAppliedQuery())
            } else {
                contentView.exitManifest()
                // §7.2: the first Esc only clears the query — the morph is still
                // open, so the field must stay the typing target (otherwise the
                // second Esc never reaches it and the field cannot leave). Focus
                // returns to the browse view when the search is closed
                // (Cancel, or the ✕-then-Esc path).
                if (!pathBar.searchActive)
                    contentView.forceActiveFocus()
            }
        }

        function onResultsChanged(summary) {
            // Read both values from the controller's properties rather than from
            // the signal argument alone. The no-results copy names the *applied*
            // query, which the view only received once at enterManifest, so a
            // query typed after the Manifest appeared would otherwise render
            // stale copy ("No matches for "sun"" while the field reads "sunxyz").
            contentView.summaryText = window.searchSummaryText(summary)
            contentView.searchQuery = window.searchAppliedQuery()
        }

        function onScopeChanged() {
            // Keep PathBar's scope chip in sync with SearchController
            if (pathBar) {
                pathBar.searchSubfolders = window.searchController.scopeRecursive()
            }
        }
    }

    // Cinema / Stage focus context (DESIGN.md §9.3): entering Cinema hands
    // keyboard focus to the Stage (transport keys), leaving it returns focus to
    // the browser so arrow navigation resumes.
    Connections {
        target: window.playbackController
        function onCinemaActiveChanged() {
            if (window.cinema) {
                // §4.2: fullscreen the existing window — never a new one.
                if (window.visibility !== Window.FullScreen) {
                    window.windowedVisibility = window.visibility
                }
                window.visibility = Window.FullScreen
                stage.forceActiveFocus()
                // §4.4: both overlays are visible on entry.
                if (stage.wakeControls)
                    stage.wakeControls()
            } else {
                window.visibility = (window.windowedVisibility === Window.FullScreen)
                                    ? Window.Windowed : window.windowedVisibility
                contentView.forceActiveFocus()
            }
        }
        // The Reel opens with playback and closes when playback ends (§10.2).
        function onPlaybackStarted(path) { window.reelVisible = true }
        function onPlaybackEnded() { window.reelVisible = false }
        // Prev/Next stopped at a folder boundary (§23.8): surface it.
        function onEndOfFolder() { toastHost.show(qsTr("End of folder"), "", "") }
    }

    // ======================================================================
    // Central keyboard map (REMEDIATION-012 / D-11; DESIGN.md §13.2–§13.3)
    // ----------------------------------------------------------------------
    // ONE declaration per key sequence; each forwards its action id to the
    // KeyboardFramework router (window.keyboard.dispatch). The map is central so
    // no two components declare the same key and no key activates twice.
    //
    // Text-entry suppression is uniform: every shortcut is disabled while
    // window.textEditing is true, so typing in the path/search/rename fields is
    // never swallowed. Context: browse keys are live outside Cinema; playback
    // keys are live only while the Stage holds focus (window.stageKeys).
    //
    // Selection/range/type-ahead keys (arrows/Home/End/PageUp/PageDown,
    // Ctrl+A, Ctrl+Space, printable type-to-select) are NOT here — ContentView +
    // SelectionState own them (REMEDIATION-013).
    // ======================================================================

    // --- Navigation (browse) ------------------------------------------------
    Shortcut { objectName: "kbNavBack";    sequence: "Alt+Left";  enabled: !window.cinema && !window.textEditing; onActivated: window.routeKey("nav.back") }
    Shortcut { objectName: "kbNavForward"; sequence: "Alt+Right"; enabled: !window.cinema && !window.textEditing; onActivated: window.routeKey("nav.forward") }
    Shortcut { objectName: "kbNavParent";  sequence: "Alt+Up";    enabled: !window.cinema && !window.textEditing; onActivated: window.routeKey("nav.parent") }
    Shortcut { objectName: "kbParentBackspace"; sequence: "Backspace"; enabled: !window.cinema && !window.textEditing; onActivated: window.routeKey("nav.parent") }
    Shortcut { objectName: "kbRefresh";    sequence: "F5";        enabled: !window.cinema && !window.textEditing; onActivated: window.routeKey("nav.refresh") }
    Shortcut { objectName: "kbHome";       sequence: "Alt+Home";  enabled: !window.cinema && !window.textEditing; onActivated: window.routeKey("nav.home") }
    Shortcut { objectName: "kbLocation";   sequence: "Ctrl+L";    enabled: !window.cinema && !window.textEditing; onActivated: window.routeKey("nav.location") }

    // --- View toggles -------------------------------------------------------
    Shortcut { objectName: "kbSearch";     sequence: "Ctrl+F";    enabled: !window.cinema && !window.textEditing; onActivated: window.routeKey("view.search") }
    Shortcut { objectName: "kbOrder";      sequence: "Ctrl+O";    enabled: !window.cinema && !window.textEditing; onActivated: window.routeKey("view.order") }
    Shortcut { objectName: "kbPlaces";     sequence: "F9";        enabled: !window.cinema && !window.textEditing; onActivated: window.routeKey("view.places") }
    Shortcut { objectName: "reelToggleShortcut"; sequence: "Ctrl+R"; enabled: window.reelModel !== null && !window.cinema && !window.textEditing; onActivated: window.routeKey("view.reel") }
    Shortcut { objectName: "kbDensity1";   sequence: "Ctrl+1";    enabled: !window.cinema && !window.textEditing; onActivated: window.routeKey("view.density1") }
    Shortcut { objectName: "kbDensity2";   sequence: "Ctrl+2";    enabled: !window.cinema && !window.textEditing; onActivated: window.routeKey("view.density2") }
    Shortcut { objectName: "kbDensity3";   sequence: "Ctrl+3";    enabled: !window.cinema && !window.textEditing; onActivated: window.routeKey("view.density3") }

    // --- Focused-row file actions ------------------------------------------
    Shortcut { objectName: "kbOpen";       sequence: "Return";    enabled: !window.cinema && !window.textEditing; onActivated: window.routeKey("item.open") }
    Shortcut { objectName: "kbRename";     sequence: "F2";        enabled: !window.cinema && !window.textEditing; onActivated: window.routeKey("item.rename") }
    Shortcut { objectName: "kbTrash";      sequence: "Delete";    enabled: !window.cinema && !window.textEditing; onActivated: window.routeKey("item.trash") }
    Shortcut { objectName: "kbDeletePermanent"; sequence: "Shift+Delete"; enabled: !window.cinema && !window.textEditing; onActivated: window.routeKey("item.deletePermanent") }
    Shortcut { objectName: "kbFavorite";   sequence: "Ctrl+Shift+F"; enabled: !window.cinema && !window.textEditing; onActivated: window.routeKey("item.favorite") }
    Shortcut { objectName: "kbWatched";    sequence: "Ctrl+Shift+W"; enabled: !window.cinema && !window.textEditing; onActivated: window.routeKey("item.watched") }
    Shortcut { objectName: "kbReveal";     sequence: "Ctrl+Shift+R"; enabled: !window.cinema && !window.textEditing; onActivated: window.routeKey("item.reveal") }
    Shortcut { objectName: "kbProperties"; sequence: "Alt+Return"; enabled: !window.cinema && !window.textEditing; onActivated: window.routeKey("item.properties") }

    // --- Clipboard / drag-drop keyboard actions (REM-014 / D-27) ---------------
    // Clipboard works whenever a selection exists — not only during playback
    // (§13.2 lists Cut/Copy with no playback precondition), and it resolves
    // identities to paths instead of treating them as row indexes.
    Shortcut { objectName: "kbCut"; sequence: "Ctrl+X"; enabled: !window.cinema && !window.textEditing && window.selectionState && window.selectionState.selectedCount > 0; onActivated: {
        const paths = contentView.pathsForIdentities(
                          window.selectionState ? window.selectionState.selectedIdentities : [])
        if (paths.length > 0 && window.actionsController)
            window.actionsController.cut(paths)
    } }
    Shortcut { objectName: "kbCopy"; sequence: "Ctrl+C"; enabled: !window.cinema && !window.textEditing && window.selectionState && window.selectionState.selectedCount > 0; onActivated: {
        const paths = contentView.pathsForIdentities(
                          window.selectionState ? window.selectionState.selectedIdentities : [])
        if (paths.length > 0 && window.actionsController)
            window.actionsController.copyToClipboard(paths)
    } }
    Shortcut { objectName: "kbPaste"; sequence: "Ctrl+V"; enabled: !window.cinema && !window.textEditing; onActivated: {
        if (window.actionsController && window.actionsController.clipboardPaths && window.actionsController.clipboardPaths().length > 0) {
            const targetDir = window.browserController ? (window.browserController.currentPath || "") : ""
            const paths = window.actionsController.clipboardPaths ? window.actionsController.clipboardPaths() : []
            if (paths.length > 0 && targetDir.length > 0) {
                window.actionsController.paste(targetDir)
            }
        }
    } }

    // --- Playback (Stage focused / Cinema) ---------------------------------
    Shortcut { objectName: "kbPlayPauseSpace"; sequence: "Space"; enabled: window.stageKeys; onActivated: window.routeKey("playback.playPause") }
    Shortcut { objectName: "kbPlayPauseK";  sequence: "K"; enabled: window.stageKeys; onActivated: window.routeKey("playback.playPause") }
    Shortcut { objectName: "kbSeekBackJ";   sequence: "J"; enabled: window.stageKeys; onActivated: window.routeKey("playback.seekBack") }
    Shortcut { objectName: "kbSeekForwardL"; sequence: "L"; enabled: window.stageKeys; onActivated: window.routeKey("playback.seekForward") }
    Shortcut { objectName: "kbSeekBackLeft"; sequence: "Left"; enabled: window.stageKeys; onActivated: window.routeKey("playback.seekBack") }
    Shortcut { objectName: "kbSeekForwardRight"; sequence: "Right"; enabled: window.stageKeys; onActivated: window.routeKey("playback.seekForward") }
    Shortcut { objectName: "kbPrevShiftLeft"; sequence: "Shift+Left"; enabled: window.stageKeys; onActivated: window.routeKey("playback.previous") }
    Shortcut { objectName: "kbNextShiftRight"; sequence: "Shift+Right"; enabled: window.stageKeys; onActivated: window.routeKey("playback.next") }
    Shortcut { objectName: "kbPrevComma";   sequence: ","; enabled: window.stageKeys; onActivated: window.routeKey("playback.previous") }
    Shortcut { objectName: "kbNextPeriod";  sequence: "."; enabled: window.stageKeys; onActivated: window.routeKey("playback.next") }
    Shortcut { objectName: "kbVolumeUp";    sequence: "Up"; enabled: window.stageKeys; onActivated: window.routeKey("playback.volumeUp") }
    Shortcut { objectName: "kbVolumeDown";  sequence: "Down"; enabled: window.stageKeys; onActivated: window.routeKey("playback.volumeDown") }
    Shortcut { objectName: "kbMute";        sequence: "M"; enabled: window.stageKeys; onActivated: window.routeKey("playback.mute") }
    Shortcut { objectName: "kbJump0"; sequence: "0"; enabled: window.stageKeys; onActivated: window.routeKey("playback.jump0") }
    Shortcut { objectName: "kbJump1"; sequence: "1"; enabled: window.stageKeys; onActivated: window.routeKey("playback.jump1") }
    Shortcut { objectName: "kbJump2"; sequence: "2"; enabled: window.stageKeys; onActivated: window.routeKey("playback.jump2") }
    Shortcut { objectName: "kbJump3"; sequence: "3"; enabled: window.stageKeys; onActivated: window.routeKey("playback.jump3") }
    Shortcut { objectName: "kbJump4"; sequence: "4"; enabled: window.stageKeys; onActivated: window.routeKey("playback.jump4") }
    Shortcut { objectName: "kbJump5"; sequence: "5"; enabled: window.stageKeys; onActivated: window.routeKey("playback.jump5") }
    Shortcut { objectName: "kbJump6"; sequence: "6"; enabled: window.stageKeys; onActivated: window.routeKey("playback.jump6") }
    Shortcut { objectName: "kbJump7"; sequence: "7"; enabled: window.stageKeys; onActivated: window.routeKey("playback.jump7") }
    Shortcut { objectName: "kbJump8"; sequence: "8"; enabled: window.stageKeys; onActivated: window.routeKey("playback.jump8") }
    Shortcut { objectName: "kbJump9"; sequence: "9"; enabled: window.stageKeys; onActivated: window.routeKey("playback.jump9") }
    // Fullscreen (F11 / F) and stop/exit (Esc) — objectNames preserved for tests.
    Shortcut { objectName: "cinemaToggleShortcut"; sequence: "F11"; enabled: window.sessionActive && !window.textEditing; onActivated: window.keyboard ? window.keyboard.dispatch("playback.fullscreen") : window.toggleCinema() }
    Shortcut { objectName: "stageFSShortcut"; sequence: "F"; enabled: window.sessionActive && !window.textEditing && (window.cinema || stage.activeFocus); onActivated: window.keyboard ? window.keyboard.dispatch("playback.fullscreen") : window.toggleCinema() }
    Shortcut { objectName: "cinemaEscShortcut"; sequence: "Esc"; enabled: window.sessionActive && !window.textEditing; onActivated: window.keyboard ? window.keyboard.dispatch("playback.stop") : window.escapePlayback() }

    // Intent handlers for the router's shell-owned surfaces (REMEDIATION-012).
    Connections {
        target: window.keyboard
        function onLocationFocusRequested() { pathBar.beginEntry() }
        function onSearchFocusRequested() {
            if (pathBar.searchActive)
                pathBar.exitSearch()
            else
                pathBar.toggleSearch()
        }
        function onOrderMenuRequested() { pathBar.openOrderMenu() }
        function onPlacesToggleRequested() { window.placesVisible = !window.placesVisible }
        function onReelToggleRequested() { window.reelVisible = !window.reelVisible }
        function onDensityRequested(density) {
            if (window.browserController && window.browserController.setDensityValue !== undefined)
                window.browserController.setDensityValue(density)
        }
        function onOpenRequested(index) { contentView.activateIndex(index) }
        function onRenameRequested(index) { contentView.beginRename(index) }
        function onTrashRequested(index) { window.trashAt(index) }
        function onPermanentDeleteRequested(index) { window.requestPermanentDelete(index) }
        function onFavoriteToggleRequested(index) { window.toggleFavoriteAt(index) }
        function onWatchedToggleRequested(index) { window.toggleWatchedAt(index) }
        function onRevealRequested(index) {
            if (window.actionsController)
                window.actionsController.reveal(contentView.pathForIndex(index))
        }
        function onPropertiesRequested(index) { window.openProperties(index) }
    }

    // Action outcomes (DESIGN.md §11.1): a failed file action must surface, not
    // disappear. `ActionsController::actionFinished` carries the real success
    // flag reported by FileOperationsService (no phantom success).
    Connections {
        target: window.actionsController
        function onActionFinished(kind, success) {
             if (!success) {
                if (kind === "rename") {
                    window.clearPendingRename()
                }

                if (kind === "delete") {
                    window.pendingPermanentPaths = []
                    window.pendingPermanentIdentities = []
                }

                toastHost.show(qsTr("%1 failed.").arg(kind), "", "")
                return
            }
            //             // Successful rename: apply the recorded destination using
            // the file's stable identity, not its potentially stale row.
            if (kind === "rename" && window.pendingRenameIndex >= 0) {
                const oldPath = window.pendingRenamePath
                const newPath = window.pendingRenameTargetPath
                const identity = window.pendingRenameIdentity

                if (identity.length > 0 && newPath.length > 0) {
                    const newName =
                        newPath.substring(newPath.lastIndexOf("/") + 1)

                    if (window.folderModel) {
                        window.folderModel.updatePathAndName(
                            identity, newPath, newName)
                    }

                    if (window.stateController) {
                        window.stateController.renamePath(
                            oldPath, newPath, identity)
                    }

                    if (window.playbackController &&
                        window.playbackController.currentIdentity() === identity) {
                        window.playbackController.refreshContext()
                    }
                }

                window.clearPendingRename()
            }
            // Successful trash: remove the row from the model.
            if (kind === "trash" && window.pendingTrashIndex >= 0 &&
                window.folderModel) {
                const identity = window.pendingTrashIdentity
                if (identity.length > 0) {
                    window.folderModel.removeByIdentity(identity)
                }
                window.pendingTrashIndex = -1
                window.pendingTrashIdentity = ""
            }
            // Successful permanent delete: drop the row(s) via a targeted
            // rowsRemoved (idempotent — the directory watcher may also diff).
            if (kind === "delete" && window.folderModel &&
                window.pendingPermanentIdentities.length > 0) {
                const identities = window.pendingPermanentIdentities
                for (let i = 0; i < identities.length; ++i) {
                    if (identities[i].length > 0) {
                        window.folderModel.removeByIdentity(identities[i])
                    }
                }
                window.pendingPermanentPaths = []
                window.pendingPermanentIdentities = []
                const n = identities.length
                toastHost.show(n === 1
                    ? qsTr("Deleted permanently")
                    : qsTr("%1 items deleted permanently").arg(n), "", "")
            }
        }
        // Operation feedback (rename, Trash + Undo, bulk partial, error) with
        // its undo affordance; the toast owns the message lifetime, the
        // controller owns the undo record (D-08).
        function onToastRequested(message, undoKind, undoPath) {
            toastHost.show(message, undoKind, undoPath)
        }
    }

    // Browser enumeration / access failures feed the content-state overlay
    // (DESIGN.md §12); navigating away clears them.
    Connections {
        target: window.browserController
        function onErrorOccurred(path, message) {
            window.browserLastError = message
            window.browserAccessDenied = message.toLowerCase().indexOf("denied") !== -1
        }
        function onCurrentPathChanged() {
            window.browserLastError = ""
            window.browserAccessDenied = false
        }
        function onParentRestored(childName) {
            // Focus restoration (§13.3 / NAV-05): when parent() navigates up,
            // restore keyboard focus to the folder that was just left.
            if (window.selectionState && window.folderModel && childName.length > 0) {
                const total = window.folderModel.count
                for (let r = 0; r < total; ++r) {
                    const name = window.folderModel.nameAt ? window.folderModel.nameAt(r) : ""
                    if (name === childName) {
                        window.selectionState.setFocusIndex(r)
                        break
                    }
                }
            }
        }
    }

    // --- Dialogs (DESIGN.md §11.1, §11.3) ------------------------------------
    PropertiesDialog {
        id: propertiesDialog
        objectName: "propertiesDialog"
        visible: false
        onCloseRequested: visible = false
    }

    RenameConflictDialog {
        id: renameConflictDialog
        objectName: "renameConflictDialog"
        visible: false
        onOverwriteConfirmed: {
            renameConflictDialog.visible = false
            // The file-operations layer refuses to overwrite (renameSync guards
            // against it); say so instead of destroying the existing file.
            toastHost.show(qsTr("Overwrite is not available yet."), "", "")
            window.clearPendingRename()
        }
        onKeepBothConfirmed: {
            renameConflictDialog.visible = false
            window.applyKeepBothRename()
        }
        onCancelConfirmed: {
            renameConflictDialog.visible = false
            window.clearPendingRename()
        }
    }

    ConfirmDeleteDialog {
        id: confirmDeleteDialog
        objectName: "confirmDeleteDialog"
        visible: false
        onConfirmed: {
            confirmDeleteDialog.visible = false
            if (window.actionsController && window.pendingPermanentPaths.length > 0) {
                // Confirmation is the ONLY route to the destructive backend:
                // dispatch the captured paths, then let actionFinished apply the
                // targeted model removal and report the real outcome.
                window.actionsController.deletePermanently(window.pendingPermanentPaths.slice())
            }
        }
        onCancelled: confirmDeleteDialog.visible = false
    }

    // --- Context menu (DESIGN.md §11.1–§11.2) --------------------------------
    // Built from ActionsController::menuItemsForSelection(count) — the controller
    // owns which verbs exist and their order; the presentation below owns the
    // glyph, shortcut, grouping (dividers) and the coral destructive styling.
    // Rebuilt per open because multi-selection changes both the title row and
    // the available verbs.
    Component {
        id: menuItemComponent
        MenuItem {
            id: menuItem
            objectName: "menuItem"
            // Presentation extras (DESIGN.md §11.2): the shortcut gutter and the
            // coral colour reserved for destructive verbs.
            property string shortcutLabel: ""
            property bool destructive: false
            height: 30

            contentItem: RowLayout {
                spacing: Theme.space3

                Text {
                    Layout.fillWidth: true
                    text: menuItem.text
                    elide: Text.ElideRight
                    color: !menuItem.enabled ? Theme.textLow
                                             : (menuItem.destructive ? Theme.danger : Theme.textHi)
                    font.pixelSize: Theme.fontBody
                }

                Text {
                    visible: menuItem.shortcutLabel.length > 0
                    text: menuItem.shortcutLabel
                    color: Theme.textLow
                    font.pixelSize: Theme.fontMicro
                }
            }
        }
    }

    Component {
        id: menuSeparatorComponent
        MenuSeparator {
            objectName: "menuSeparator"
        }
    }

    Menu {
        id: contentMenu
        objectName: "contentMenu"
        property int targetIndex: -1
    }

    // Presentation table keyed by the label ActionsController returns. Shortcuts
    // mirror DESIGN.md §13.2; `bulk` marks verbs that survive multi-selection;
    // `group` drives the dividers (verbs ahead of destructive actions).
    readonly property var menuPresentation: ({
        "Play": { iconName: "play", shortcut: "", group: 1, destructive: false, bulk: false, enabled: true },
        "Favorite": { iconName: "favorite", shortcut: "Ctrl+Shift+F", group: 1, destructive: false, bulk: true, enabled: true },
        "Mark Watched": { iconName: "watched", shortcut: "Ctrl+Shift+W", group: 1, destructive: false, bulk: true, enabled: true },
        "Rename": { iconName: "edit", shortcut: "F2", group: 2, destructive: false, bulk: false, enabled: true },
        "Cut": { iconName: "cut", shortcut: "Ctrl+X", group: 2, destructive: false, bulk: true, enabled: true },
        "Copy": { iconName: "copy", shortcut: "Ctrl+C", group: 2, destructive: false, bulk: true, enabled: true },
        "Reveal": { iconName: "reveal", shortcut: "Ctrl+Shift+R", group: 2, destructive: false, bulk: false, enabled: true },
        "Open With": { iconName: "open-with", shortcut: "", group: 2, destructive: false, bulk: false, enabled: true },
        "Add to Collection (coming soon)": { iconName: "", shortcut: "", group: 2, destructive: false, bulk: true, enabled: false },
        "Properties": { iconName: "properties", shortcut: "Alt+Enter", group: 3, destructive: false, bulk: false, enabled: true },
        "Move to Trash": { iconName: "trash", shortcut: "Del", group: 4, destructive: true, bulk: true, enabled: true },
        "Delete Permanently": { iconName: "delete-permanent", shortcut: "Shift+Del", group: 4, destructive: true, bulk: true, enabled: true }
    })

    function menuSpec(label) {
        const spec = window.menuPresentation[label]
        return spec ? spec : { iconName: "", shortcut: "", group: 9, destructive: false, bulk: false, enabled: true }
    }

    function isMenuTitle(label) {
        return /^[0-9]+ items$/.test(label)
    }

    function rebuildContextMenu(selectionCount) {
        for (let i = contentMenu.count - 1; i >= 0; --i)
            contentMenu.removeItem(contentMenu.itemAt(i))

        // Items must be created with a visual parent (the popup's contentItem),
        // otherwise Qt warns that the object never entered the graphics scene.
        const host = contentMenu.contentItem !== null ? contentMenu.contentItem : contentMenu

        const labels = window.actionsController
                       ? window.actionsController.menuItemsForSelection(selectionCount) : []
        let lastGroup = -1
        let emitted = 0
        for (let i = 0; i < labels.length; ++i) {
            const label = labels[i]
            if (isMenuTitle(label)) {
                // Multi-selection title row ("3 items"), disabled.
                contentMenu.addItem(menuItemComponent.createObject(host,
                    { text: label, enabled: false }))
                continue
            }
            const spec = menuSpec(label)
            // Multi-selection keeps only the bulk-capable verbs (§11.2).
            if (selectionCount > 1 && !spec.bulk)
                continue
            // Dividers only *between* verb groups (never before the first):
            // verbs ahead of the destructive pair (DESIGN §11.1).
            if (spec.group !== lastGroup && emitted > 0)
                contentMenu.addItem(menuSeparatorComponent.createObject(host))
            lastGroup = spec.group
            const itemProps = {
                text: label,
                enabled: spec.enabled,
                shortcutLabel: spec.shortcut,
                destructive: spec.destructive
            }
            if (spec.iconName && spec.iconName.length > 0) {
                itemProps["icon.source"] = "qrc:/qt/qml/qg/icons/" + spec.iconName + ".svg"
                itemProps["icon.color"] = spec.destructive ? Theme.danger : Theme.textHi
            }
            const item = menuItemComponent.createObject(host, itemProps)
            item.triggered.connect(function () { window.dispatchMenuAction(label) })
            contentMenu.addItem(item)
            emitted += 1
        }
    }

    // --- Cinema cursor auto-hide + any-input wake (DESIGN.md §9.2, §25.7) -----
    MouseArea {
        id: cinemaOverlay
        objectName: "cinemaOverlay"
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.NoButton
        z: 100
        visible: window.cinema

        // Cursor hides after 2s idle in Cinema (unless textEditing); any mouse
        // movement or click restores it and wakes controls.
        cursorShape: (window.cinema && !window.textEditing && !cursorIdleTimer.running)
                     ? Qt.BlankCursor : Qt.ArrowCursor

        onPositionChanged: {
            cursorIdleTimer.restart()
            if (window.cinema && stage && stage.wakeControls)
                stage.wakeControls()
        }
        onClicked: {
            cursorIdleTimer.restart()
            if (window.cinema && stage && stage.wakeControls)
                stage.wakeControls()
        }
    }

    Timer {
        id: cursorIdleTimer
        objectName: "cursorIdleTimer"
        interval: 2000
        repeat: false
        running: window.cinema && !window.textEditing
        onTriggered: {
            // Cursor hides via cinemaOverlay.cursorShape binding.
        }
    }
}
