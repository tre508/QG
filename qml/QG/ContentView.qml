import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import qg

// Content surface grid host (DESIGN.md §3.4, §3.5, §5.4, §5.5).
//
// A virtualized presentation over the canonical model with three densities:
// Gallery/Standard use a GridView of folder/video cards; Compact uses a ListView
// of CompactRow. Responsive columns, a keyboard focus cursor (cyan, inset)
// independent of selection (signal), and scroll management. Presentation only.

Item {
    id: contentView

    objectName: "contentView"
    property var model: null
    property var selectionState: null
    property var typeToSelectBuffer: null

    enum Density { Gallery, Standard, Compact }
    property int density: ContentView.Standard

    // Legacy single-selection cursor, used only when no SelectionState is
    // injected (tests). Declared so the fallback branches are not undefined.
    property int focusIndex: -1
    property int selectedIndex: -1

    // --- Observable selection/focus state (AUD-004 / FIX-SEL-01) -------------
    // A QML binding only re-evaluates when a *property* it read changes. The
    // previous invokable-only reads (`selectionState.focusIndex()`,
    // `selectionState.isSelected(...)`) created no dependency, so card borders,
    // the cyan focus ring, the status strip and the context-menu title were
    // computed once at delegate creation and never again. These two adapters
    // read the real SelectionState's properties (creating the dependency) and
    // still accept a test double that exposes the same state as invokables.
    readonly property int selFocusIndex: {
        if (!selectionState)
            return focusIndex
        const v = selectionState.focusIndex
        return typeof v === "function" ? selectionState.focusIndex() : v
    }
    // Monotonic selection revision: read by per-identity predicates so they have
    // something to depend on (`isSelected()` alone notifies nothing).
    readonly property int selRevision: {
        if (!selectionState)
            return 0
        const v = selectionState.selectionRevision
        if (typeof v === "function")
            return selectionState.selectionRevision()
        return (v === undefined || v === null) ? 0 : v
    }

    readonly property bool compact: density === ContentView.Compact
    readonly property int itemCount: compact ? compactList.count
                                    : (manifest ? manifestList.count : grid.count)

    // Manifest mode (Prompt 43/45): search results presentation via ListView
    property bool manifest: false
    property string searchQuery: ""
    property var searchProxy: null  // SearchProxy instance for matching
    property string summaryText: ""

    // Recursive search integration
    property var recursiveModel: null   // RecursiveSearchModel from SearchController
    property bool searchRecursive: false // Bound to SearchController.scopeRecursive()

    // Scroll/selection preservation across manifest toggle
    property real savedGridViewY: 0

    // §7.4: the no-results overlay's "Clear search" is an intent, not a view
    // action — the query belongs to SearchController, so the view forwards it to
    // the shell. Leaving the Manifest here on its own left the morph open and the
    // controller active over a Browse grid.
    signal searchClearRequested()

    function enterManifest(proxy, query) {
        savedGridViewY = grid.visible ? grid.contentY : compactList.contentY
        searchProxy = proxy
        searchQuery = query
        manifest = true
        if (selectionState) {
            selectionState.setFocusIndex(0)
            selectionState.clear()
        }
    }

    function exitManifest() {
        manifest = false
        searchProxy = null
        searchQuery = ""
        summaryText = ""   // no stale summary after leaving search
        // Restore scroll position
        if (grid.visible) grid.contentY = savedGridViewY
        else compactList.contentY = savedGridViewY
    }

    // --- Authoritative grid geometry (DESIGN.md §3.5, §5.4) -----------------
    // This block is the ONLY place card/column geometry is computed; delegates
    // read `cardWidth`/`cellHeight` and never derive sizes themselves.
    //
    // §3.5: "Column count is max(1, floor((content_width - gutters) /
    // (min_card + gap))) with min_card = 168 and max_card = 320. Cards never
    // stretch beyond max_card; extra width becomes gutters at the row edges
    // (centered), not stretched thumbnails." §3.5 further gives a per-band
    // target card width (240 at the wide layout) and §5.4 calls Standard
    // "3–6 columns typical" with a full metadata line.
    //
    // Deriving the column count from min_card alone maximises columns and makes
    // cards much narrower than the target (VIS-008). The count is therefore
    // derived from the density TARGET: the largest number of columns whose cards
    // are still at least that target, clamped so cards never fall below
    // min_card and never exceed max_card.
    readonly property int pad: 24
    readonly property int gap: 16

    // Responsive target supplied by the shell's §3.5 table (Main.qml). The
    // default is the wide-layout value so the view is deterministic standalone.
    property int targetCardWidth: 240

    // Vertical space owned by an inline surface above the grid (the Stage).
    // The shell binds this to the Stage's height so the cards reflow beneath it
    // instead of being covered (DESIGN.md §3.4, playback §2.1.7, §2.2).
    property real reservedTop: 0

    // Effective top inset of every presentation: the standard pad plus whatever
    // an inline surface above the grid has reserved.
    readonly property real topInset: pad + reservedTop
    readonly property int targetCard: density === ContentView.Gallery ? 340 : targetCardWidth
    readonly property int minCard: density === ContentView.Gallery ? 240 : 168
    readonly property int maxCard: density === ContentView.Gallery ? 360 : 320

    // Usable width for cards (logical pixels; DPR is never multiplied in here).
    readonly property int availableWidth: Math.max(0, width - 2 * pad)

    // Card width when `n` columns share `availableWidth`.
    function cardWidthAt(n) {
        if (n <= 0 || availableWidth <= 0)
            return minCard
        return Math.floor((availableWidth - (n - 1) * gap) / n)
    }

    // Never narrower than minCard, never fewer than one column.
    readonly property int maxColumns: Math.max(1, Math.floor((availableWidth + gap) / (minCard + gap)))

    readonly property int columns: {
        // Largest count whose cards still reach the density target.
        const byTarget = Math.floor((availableWidth + gap) / (targetCard + gap))
        if (!isFinite(byTarget) || byTarget < 1)
            return 1
        return Math.min(maxColumns, byTarget)
    }

    readonly property int cardWidth: {
        if (availableWidth <= 0)
            return 1                       // degenerate: never negative, never divide by zero
        const ceiling = Math.min(maxCard, availableWidth)   // never overflow the viewport
        return Math.max(1, Math.min(ceiling, Math.max(minCard, cardWidthAt(columns))))
    }

    // Width actually occupied by the cards (trailing cell gap excluded).
    readonly property int occupiedWidth: columns * cardWidth + (columns - 1) * gap
    // Symmetric side gutter: leftover width is distributed equally (§3.5).
    readonly property real gridGutter: Math.max(0, (width - occupiedWidth) / 2)

    readonly property int thumbHeight: Math.round(cardWidth * 9 / 16)
    // Fixed metadata band (DESIGN.md §6): text length never changes row height.
    readonly property int metaBandHeight: 44
    readonly property int cellHeight: thumbHeight + metaBandHeight

    signal activated(int index)
    signal activatedItem(int index, string path, bool isFolder)
    signal selectionChanged(int index)

    // The visible row range (canonical model indices) changed — on scroll,
    // resize, density change, or a new population. Main.qml routes this to the
    // thumbnail/probe controllers so only visible items trigger work
    // (DESIGN.md §18.3, §22.4; REM-01). Emitted only while Browse/Compact is
    // shown: the Manifest lists the filtered proxy, whose rows are not model
    // indices, so its (secondary) thumbnails ride the browse feed.
    signal visibleRangeChanged(int first, int last)

    // Manifest header sort (DESIGN §7.3 / SEARCH-04): routes to Order menu.
    signal sortRequested(int key, bool descending, bool foldersFirst)
    readonly property int sortKeyValue: window.browserController ? window.browserController.sortKey : 0
    readonly property bool sortDescendingValue: window.browserController ? window.browserController.sortDescending : false

    // Card action intents (DESIGN.md §6, §11.1–§11.3). Presentation forwards them
    // with the row index; Main.qml routes them to the composed controllers
    // (REM-009 / D-04). Nothing is emitted without an index.
    signal playRequested(int index)
    signal favoriteToggled(int index)
    signal watchedToggled(int index)
    signal renameCommitted(int index, string newName)
    signal overflowRequested(int index, real globalX, real globalY)
    signal thumbnailRetryRequested(int index)
    // Destructive intents (DESIGN.md §11.1): `Delete` trashes (undoable);
    // `Shift+Delete` asks for permanent deletion (the confirm dialog).
    signal trashRequested(int index)
    signal permanentDeleteRequested(int index)

    // Inline-rename target row (-1 = none); the delegate binds `renaming`.
    property int renameIndex: -1
    // Double-click suppression: a burst of activations must dispatch ONE play
    // (a click+double-click pair, or an impatient double-double-click).
    property int lastActivatedIndex: -1
    property double lastActivationMs: 0
    readonly property int activationGuardMs: 300

    function identityForIndex(index) {
        return (model && model.identityAt) ? model.identityAt(index) : ""
    }

    // Identity of the row at `index` in the *current view order*. Browse rows are
    // canonical model rows; Manifest rows are proxy rows, so their identity has
    // to come from the model that produced them (SearchProxy maps a proxy row
    // back to its source FolderModel row) — resolving a Manifest row through the
    // browse model would select an unrelated entry.
    function identityAt(index) {
        if (index < 0)
            return ""
        if (manifest) {
            const src = manifestList.model
            if (src && typeof src.identityAt === "function")
                return src.identityAt(index)
            return ""
        }
        return identityForIndex(index)
    }

    // Every identity in the current view order (Ctrl+A, §13.2). Entries without
    // an identity (an unresolved recursive hit) are skipped: selection is
    // identity-keyed, and an empty key would match everything.
    function allIdentities() {
        const ids = []
        for (let i = 0; i < itemCount; ++i) {
            const id = identityAt(i)
            if (id && id.length > 0)
                ids.push(id)
        }
        return ids
    }

    // Identities of the inclusive index range [from, to], in view order. Only the
    // view knows this order, which is why range selection is built here and not
    // in SelectionState.
    function identitiesInRange(from, to) {
        const ids = []
        if (itemCount <= 0)
            return ids
        const lo = Math.max(0, Math.min(from, to))
        const hi = Math.min(itemCount - 1, Math.max(from, to))
        for (let i = lo; i <= hi; ++i) {
            const id = identityAt(i)
            if (id && id.length > 0)
                ids.push(id)
        }
        return ids
    }

    function beginRename(index) {
        if (index >= 0 && index < itemCount)
            renameIndex = index
    }

    // Forwarding helpers so every card intent is dispatchable from a test
    // without synthesising delegate clicks.
    function requestPlayAt(index) { activateIndex(index) }
    function requestFavoriteAt(index) { if (index >= 0) favoriteToggled(index) }
    function requestWatchedAt(index) { if (index >= 0) watchedToggled(index) }
    function requestOverflowAt(index, globalX, globalY) {
        if (index >= 0)
            overflowRequested(index, globalX === undefined ? 0 : globalX,
                              globalY === undefined ? 0 : globalY)
    }
    function requestThumbnailRetryAt(index) { if (index >= 0) thumbnailRetryRequested(index) }
    function requestTrashAt(index) { if (index >= 0) trashRequested(index) }
    function requestPermanentDeleteAt(index) { if (index >= 0) permanentDeleteRequested(index) }

    function pathForIndex(index) {
        return (model && model.pathAt) ? model.pathAt(index) : ""
    }

    // Identities (not indexes) in, paths out — the clipboard and the context
    // menu speak identities, so they must never be fed to pathAt().
    function pathsForIdentities(ids) {
        const out = []
        if (!model || !model.pathForIdentity || !ids)
            return out
        for (let i = 0; i < ids.length; ++i) {
            const p = model.pathForIdentity(ids[i])
            if (p && p.length > 0)
                out.push(p)
        }
        return out
    }
    function isFolderForIndex(index) {
        return (model && model.isFolderAt) ? model.isFolderAt(index) : false
    }

    function clampFocus() {
        if (!selectionState) return
        if (itemCount <= 0) {
            selectionState.setFocusIndex(-1)
            return
        }
        if (selFocusIndex < 0) {
            selectionState.setFocusIndex(0)
            return
        }
        if (selFocusIndex > itemCount - 1)
            selectionState.setFocusIndex(itemCount - 1)
    }
    onItemCountChanged: {
        clampFocus()
        reportVisibleRange()
    }

    // Compute the visible canonical-model row range for the active browse view
    // and emit it (REM-01). Grid uses cell rows × columns; Compact is one row
    // per entry (60px); Manifest is skipped (its proxy rows are not model rows).
    function reportVisibleRange() {
        if (itemCount <= 0 || manifest) {
            return
        }
        let first = 0
        let last = itemCount - 1
        if (compact) {
            const rowH = 60
            first = Math.floor(compactList.contentY / rowH)
            last = first + Math.ceil(compactList.height / rowH)
        } else {
            const rowH = cellHeight + gap
            // `contentY` is measured from the content origin, which the top inset
            // already places below the Stage — so the visible content window is
            // [contentY, contentY + height) and no inset may be subtracted here.
            // (Subtracting the margin double-counted it; with a large Stage inset
            // that dropped the first screen entirely, AUD-009.)
            const firstRow = Math.floor(grid.contentY / rowH)
            const lastRow = Math.ceil((grid.contentY + grid.height) / rowH)
            first = firstRow * columns
            last = (lastRow + 1) * columns - 1
        }
        first = Math.max(0, first)
        last = Math.min(itemCount - 1, last)
        if (last >= first) {
            visibleRangeChanged(first, last)
        }
    }

    // Density / mode changes swap which view is on screen: re-report the range.
    onColumnsChanged: reportVisibleRange()
    onManifestChanged: reportVisibleRange()
    onCompactChanged: reportVisibleRange()
    // The Stage appearing/disappearing reserves or releases vertical space, which
    // changes what the first screen shows (AUD-009).
    onReservedTopChanged: reportVisibleRange()

    function activateIndex(index) {
        if (index < 0 || index >= itemCount)
            return
        // One activation per burst: a click followed by a double-click (or a
        // rapid double-double-click) must not start playback twice (REM-009 E4).
        const now = Date.now()
        if (index === lastActivatedIndex && now - lastActivationMs < activationGuardMs)
            return
        lastActivatedIndex = index
        lastActivationMs = now
        if (selectionState) {
            selectionState.setFocusIndex(index)
        }
        activated(index)
        activatedItem(index, pathForIndex(index), isFolderForIndex(index))
    }
    // --- Focus movement (view-owned bounds) ---------------------------------
    // SelectionState cannot clamp: only the view knows the column count, the row
    // height and therefore `itemCount` (AUD-004 — a model that guesses view order
    // is the same class of defect as a range that guesses indices).
    function moveFocusTo(index) {
        if (!selectionState) return
        if (itemCount <= 0) {
            selectionState.setFocusIndex(-1)
            return
        }
        selectionState.setFocusIndex(Math.max(0, Math.min(itemCount - 1, index)))
    }
    function moveFocus(delta) {
        if (!selectionState) return
        const idx = selFocusIndex
        if (idx < 0 || itemCount <= 0)
            return
        moveFocusTo(idx + delta)
    }
    // Shift+arrow (§13.2): move the cursor, then extend the range from the anchor
    // to its new position. Previously the Shift branches extended to the *current*
    // focus without moving it, so they selected nothing new.
    function moveFocusAndExtend(delta) {
        if (!selectionState) return
        const idx = selFocusIndex
        if (idx < 0 || itemCount <= 0)
            return
        const next = Math.max(0, Math.min(itemCount - 1, idx + delta))
        selectionState.setFocusIndex(next)
        extendRangeTo(next)
    }
    function selectIndex(index) {
        if (!selectionState) return
        const identity = identityAt(index)
        if (identity.length === 0) return
        selectionState.selectOne(identity, index)
        selectionChanged(index)
    }
    function toggleSelection(index) {
        if (!selectionState) return
        const identity = identityAt(index)
        if (identity.length === 0) return
        selectionState.toggle(identity, index)
    }
    // Shift+click / Shift+arrow (§5.5): the anchor is the last click or, with no
    // click yet, the row the cursor came from; the selection becomes every row
    // between the anchor and `index` (inclusive, in view order).
    function extendRangeTo(index) {
        if (!selectionState) return
        if (index < 0 || index >= itemCount) return
        let anchor = selectionState.anchorIndex
        if (typeof anchor === "function")
            anchor = selectionState.anchorIndex()
        if (anchor === undefined || anchor === null || anchor < 0)
            anchor = (selFocusIndex >= 0 ? selFocusIndex : index)
        const ids = identitiesInRange(anchor, index)
        if (ids.length === 0) return
        selectionState.extendRangeTo(ids, index)
    }
    // Ctrl+A (§13.2): every identity in the current view order.
    function selectAll() {
        if (!selectionState) return
        const ids = allIdentities()
        if (ids.length === 0) return
        selectionState.selectAll(ids)
    }
    function clearSelection() {
        if (!selectionState) return
        selectionState.clear()
    }

    function sortBy(key) {
        // Click on active column flips direction; new column starts ascending.
        const foldersFirst = (typeof window !== "undefined" && window.browserController)
                             ? (window.browserController.foldersFirst || true) : true
        const descending = (key === sortKeyValue) ? !sortDescendingValue : false
        sortRequested(key, descending, foldersFirst)
    }

    function handleKey(event) {
        // Check if any text input has focus - suppress type-to-select and some shortcuts
        const hasTextInputFocus = (() => {
            const item = Qt.application.activeWindow ? Qt.application.activeWindow.activeFocusItem : null
            if (!item) return false
            const kind = "" + item
            return kind.indexOf("QQuickTextInput") !== -1
                    || kind.indexOf("QQuickTextEdit") !== -1
                    || item.echoMode !== undefined
        })()

        switch (event.key) {
        case Qt.Key_Left:
            if (event.modifiers & Qt.ShiftModifier) {
                moveFocusAndExtend(-1)
            } else {
                moveFocus(-1)
                if (!(event.modifiers & Qt.ControlModifier)) clearSelection()
            }
            event.accepted = true; break
        case Qt.Key_Right:
            if (event.modifiers & Qt.ShiftModifier) {
                moveFocusAndExtend(1)
            } else {
                moveFocus(1)
                if (!(event.modifiers & Qt.ControlModifier)) clearSelection()
            }
            event.accepted = true; break
        case Qt.Key_Up:
            if (event.modifiers & Qt.ShiftModifier) {
                moveFocusAndExtend(-(compact ? 1 : columns))
            } else {
                moveFocus(-(compact ? 1 : columns))
                if (!(event.modifiers & Qt.ControlModifier)) clearSelection()
            }
            event.accepted = true; break
        case Qt.Key_Down:
            if (event.modifiers & Qt.ShiftModifier) {
                moveFocusAndExtend(compact ? 1 : columns)
            } else {
                moveFocus(compact ? 1 : columns)
                if (!(event.modifiers & Qt.ControlModifier)) clearSelection()
            }
            event.accepted = true; break
        case Qt.Key_Home:
            if (event.modifiers & Qt.ShiftModifier) {
                moveFocusAndExtend(-itemCount)   // clamped to row 0 by moveFocusAndExtend
            } else if (selectionState) {
                moveFocusTo(0)
            } else {
                focusIndex = 0
            }
            event.accepted = true; break
        case Qt.Key_End:
            if (event.modifiers & Qt.ShiftModifier) {
                moveFocusAndExtend(itemCount)    // clamped to the last row
            } else if (selectionState) {
                moveFocusTo(itemCount - 1)
            } else {
                focusIndex = itemCount - 1
            }
            event.accepted = true; break
        case Qt.Key_PageUp:
            if (event.modifiers & Qt.ShiftModifier) {
                moveFocusAndExtend(-10)
            } else {
                moveFocus(-10)
            }
            event.accepted = true; break
        case Qt.Key_PageDown:
            if (event.modifiers & Qt.ShiftModifier) {
                moveFocusAndExtend(10)
            } else {
                moveFocus(10)
            }
            event.accepted = true; break
        // NOTE: Enter (open), F2 (rename), Delete / Shift+Delete (trash /
        // permanent) and Ctrl+Shift+F / Ctrl+Shift+W are routed centrally by
        // KeyboardFramework (REMEDIATION-012 / D-11), which reads this view's
        // SelectionState focus. They are deliberately NOT handled here, so a
        // single keypress can never activate twice.
        // Multi-selection shortcuts (DESIGN.md §13.2)
        case Qt.Key_A:
            if (event.modifiers & Qt.ControlModifier) {
                selectAll()
                event.accepted = true
            }
            break
        case Qt.Key_Space:
            if (event.modifiers & Qt.ControlModifier) {
                toggleSelection(selectionState ? selFocusIndex : focusIndex)
                event.accepted = true
            }
            break
        case Qt.Key_Escape:
            // §5.5 / §13.2: Esc clears the selection. While the Manifest is up,
            // Esc belongs to the search morph ("clear search" first) and while a
            // session is active Main.qml's Esc shortcut stops playback — in both
            // cases this view must leave the key alone.
            if (!manifest && selectionState) {
                clearSelection()
                event.accepted = true
            }
            break
        default:
            // Type-to-select: feed printable keys when view has focus and no text input focused
            if (!hasTextInputFocus && event.text && event.text.length === 1 && event.text.charCodeAt(0) >= 32 && event.text.charCodeAt(0) < 127) {
                if (typeToSelectBuffer) {
                    typeToSelectBuffer.addChar(event.text)
                    const buffer = typeToSelectBuffer.buffer()
                    if (buffer.length > 0 && model) {
                        // Find first matching item starting from current focus
                        const startIdx = selectionState ? selFocusIndex : focusIndex
                        for (let i = 0; i < itemCount; ++i) {
                            const idx = (startIdx + i) % itemCount
                            const name = model.nameAt ? model.nameAt(idx) : ""
                            if (name.toLowerCase().startsWith(buffer.toLowerCase())) {
                                if (selectionState) selectionState.setFocusIndex(idx)
                                else focusIndex = idx
                                // Position view at the found item
                                if (grid.visible) grid.positionViewAtIndex(idx, GridView.Contain)
                                else if (compactList.visible) compactList.positionViewAtIndex(idx, ListView.Contain)
                                else if (manifestList.visible) manifestList.positionViewAtIndex(idx, ListView.Contain)
                                break
                            }
                        }
                    }
                    event.accepted = true
                }
            }
            break
        }
    }

    // --- Grid densities (Gallery / Standard) --------------------------------
    GridView {
        id: grid
        objectName: "contentGrid"
        // The top inset is the pad plus whatever an inline surface (the Stage) has
        // reserved, so cards reflow beneath it instead of being covered (AUD-009).
        anchors.fill: parent
        anchors.topMargin: contentView.topInset
        anchors.bottomMargin: contentView.pad
        // Center the occupied grid (§3.5: leftover width becomes gutters at the
        // row edges). Only `leftMargin` is set: a non-zero `rightMargin` shrinks
        // the flow width below `columns * cellWidth`, so GridView silently drops
        // the last column while this geometry still assumed it — the source of
        // the "7 of 8 columns" and the dead band on the right (VIS-009).
        leftMargin: contentView.gridGutter
        rightMargin: 0
        cellWidth: contentView.cardWidth + contentView.gap
        cellHeight: contentView.cellHeight + contentView.gap
        cacheBuffer: 400
        clip: true
        visible: !contentView.compact && !contentView.manifest
        model: contentView.model
        focus: !contentView.compact && !contentView.manifest

        // Viewport moves / resizes feed the thumbnail+probe schedulers (REM-01).
        onContentYChanged: contentView.reportVisibleRange()
        onHeightChanged: contentView.reportVisibleRange()

        delegate: DelegateChooser {
            role: "isFolder"

            DelegateChoice {
                roleValue: false
                VideoCard {
                    objectName: "contentCell"
                    width: contentView.cardWidth
                    height: contentView.cellHeight
                    fileName: model.name
                    path: contentView.pathForIndex(index) || ""
                    durationMs: model.duration === undefined ? -1 : model.duration
                    sizeBytes: model.size === undefined ? 0 : model.size
                    mtimeNs: model.mtime === undefined ? 0 : model.mtime
                    favorite: model.favorite === true
                    watched: model.watched === true
                    progress: model.progress === undefined ? -1 : model.progress
                    thumbnailState: model.thumbnailState === undefined ? 0 : model.thumbnailState
                    thumbnail: model.thumbnail === undefined ? null : model.thumbnail
                    widthPx: model.width === undefined ? 0 : model.width
                    heightPx: model.height === undefined ? 0 : model.height
                    detailedMeta: contentView.density !== ContentView.Gallery
                    focused: index === contentView.selFocusIndex
                    // Reading the revision is what makes this binding depend on the
                    // selection model: isSelected() alone notifies nothing (AUD-004).
                    selected: selectionState
                              ? (contentView.selRevision >= 0
                                 && selectionState.isSelected(contentView.identityAt(index)))
                              : (index === contentView.selectedIndex)
                    renaming: index === contentView.renameIndex
                    validateRename: function (newBaseName) {
                        if (!window.actionsController
                                || typeof window.actionsController.validateRename !== "function")
                            return ({ ok: true, message: "" })
                        const siblings = []
                        for (let i = 0; i < contentView.itemCount; ++i) {
                            let name = ""
                            if (contentView.model && typeof contentView.model.nameAt === "function")
                                name = contentView.model.nameAt(i)
                            else if (contentView.model && typeof contentView.model.get === "function") {
                                const row = contentView.model.get(i)
                                name = row && row.name !== undefined ? row.name : ""
                            }
                            if (name && name.length > 0)
                                siblings.push(name)
                        }
                        return window.actionsController.validateRename(
                                    contentView.pathForIndex(index), newBaseName, siblings)
                    }
                    onClicked: contentView.selectIndex(index)
                    onDoubleClicked: contentView.activateIndex(index)
                    // Card actions forward with this row's index (REM-009 / D-04).
                    onPlayRequested: contentView.requestPlayAt(index)
                    onFavoriteRequested: contentView.requestFavoriteAt(index)
                    onThumbnailRetryRequested: contentView.requestThumbnailRetryAt(index)
                    onRenameCommitted: function (newName) {
                        contentView.renameCommitted(index, newName)
                    }
                    onRenameFinished: contentView.renameIndex = -1
                    onRenameCancelled: contentView.renameIndex = -1
                    onOverflowRequested: {
                        // Pop the menu where the card is, in scene coordinates.
                        const p = mapToItem(null, width, height / 2)
                        contentView.requestOverflowAt(index, p.x, p.y)
                    }
                }
            }

            DelegateChoice {
                roleValue: true
                FolderCard {
                    objectName: "contentCell"
                    width: contentView.cardWidth
                    height: contentView.cellHeight
                    folderName: model.name
                    path: contentView.pathForIndex(index) || ""
                    videoCount: model.videoCount === undefined ? 0 : model.videoCount
                    folderCount: model.folderCount === undefined ? 0 : model.folderCount
                    mosaicSources: model.mosaic === undefined ? [] : model.mosaic
                    focused: index === contentView.selFocusIndex
                    selected: selectionState
                              ? (contentView.selRevision >= 0
                                 && selectionState.isSelected(contentView.identityAt(index)))
                              : (index === contentView.selectedIndex)
                    onClicked: contentView.selectIndex(index)
                    onDoubleClicked: contentView.activateIndex(index)
                    onDropReceived: function (paths, targetDir, isMove) {
                        contentView.handleDrop(paths, targetDir, isMove)
                    }
                }
            }
        }

        Keys.onPressed: function (event) { contentView.handleKey(event) }
    }

    // --- Compact density ----------------------------------------------------
    ListView {
        id: compactList
        objectName: "compactList"
        anchors.fill: parent
        anchors.topMargin: contentView.topInset
        anchors.bottomMargin: contentView.pad
        cacheBuffer: 400
        clip: true
        visible: contentView.compact && !contentView.manifest
        model: contentView.model
        focus: contentView.compact && !contentView.manifest

        // Viewport moves / resizes feed the thumbnail+probe schedulers (REM-01).
        onContentYChanged: contentView.reportVisibleRange()
        onHeightChanged: contentView.reportVisibleRange()

        delegate: CompactRow {
            objectName: "contentCell"
            width: compactList.width
            fileName: model.name
            path: contentView.pathForIndex(index) || ""
            isFolder: model.isFolder === true
            durationMs: model.duration === undefined ? -1 : model.duration
            sizeBytes: model.size === undefined ? 0 : model.size
            mtimeNs: model.mtime === undefined ? 0 : model.mtime
            videoCount: model.videoCount === undefined ? 0 : model.videoCount
            folderCount: model.folderCount === undefined ? 0 : model.folderCount
            favorite: model.favorite === true
            watched: model.watched === true
            playing: model.playing === true
            thumbnailState: model.thumbnailState === undefined ? 0 : model.thumbnailState
            focused: index === contentView.selFocusIndex
            selected: selectionState
                      ? (contentView.selRevision >= 0
                         && selectionState.isSelected(contentView.identityAt(index)))
                      : (index === contentView.selectedIndex)
            onClicked: contentView.selectIndex(index)
            onDoubleClicked: contentView.activateIndex(index)
        }

        Keys.onPressed: function (event) { contentView.handleKey(event) }
    }

    // --- Manifest density (Prompt 43/45) — search results ListView + header
    Item {
        anchors.fill: parent
        // The sortable header and the results both start below the inset, so the
        // Manifest behaves like the other two presentations (AUD-009).
        anchors.topMargin: contentView.topInset
        visible: contentView.manifest

        // Sortable header row (synchronized with Order menu)
        Rectangle {
            id: manifestHeader
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 28
            color: Theme.surface2

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: Theme.space3
                anchors.rightMargin: Theme.space3
                spacing: Theme.space3

                // Name column — fill width; clickable; active indicator when sorted by name.
                Item {
                    Layout.fillWidth: true
                    height: parent.height
                    MouseArea {
                        anchors.fill: parent
                        onClicked: contentView.sortBy(0)  // SortKey::Name = 0
                    }
                    Row {
                        anchors.fill: parent
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: Theme.space1
                        Text {
                            text: "Name"
                            color: contentView.sortKeyValue === 0 ? Theme.signal : Theme.textLow
                            font.pixelSize: Theme.fontMicro
                            font.bold: contentView.sortKeyValue === 0
                        }
                        Text {
                            text: contentView.sortKeyValue === 0 ? (contentView.sortDescendingValue ? "\u25BC" : "\u25B2") : ""
                            color: contentView.sortKeyValue === 0 ? Theme.signal : Theme.textLow
                            font.pixelSize: Theme.fontMicro
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }

                // Duration column
                Item {
                    Layout.preferredWidth: 72
                    height: parent.height
                    MouseArea {
                        anchors.fill: parent
                        onClicked: contentView.sortBy(2)  // SortKey::Duration = 2
                    }
                    Row {
                        anchors.fill: parent
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: Theme.space1
                        Text {
                            text: "Duration"
                            color: contentView.sortKeyValue === 2 ? Theme.signal : Theme.textLow
                            font.pixelSize: Theme.fontMicro
                            font.bold: contentView.sortKeyValue === 2
                            horizontalAlignment: Text.AlignRight
                        }
                        Text {
                            text: contentView.sortKeyValue === 2 ? (contentView.sortDescendingValue ? "\u25BC" : "\u25B2") : ""
                            color: contentView.sortKeyValue === 2 ? Theme.signal : Theme.textLow
                            font.pixelSize: Theme.fontMicro
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }

                // Size column
                Item {
                    Layout.preferredWidth: 88
                    height: parent.height
                    MouseArea {
                        anchors.fill: parent
                        onClicked: contentView.sortBy(3)  // SortKey::Size = 3
                    }
                    Row {
                        anchors.fill: parent
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: Theme.space1
                        Text {
                            text: "Size"
                            color: contentView.sortKeyValue === 3 ? Theme.signal : Theme.textLow
                            font.pixelSize: Theme.fontMicro
                            font.bold: contentView.sortKeyValue === 3
                            horizontalAlignment: Text.AlignRight
                        }
                        Text {
                            text: contentView.sortKeyValue === 3 ? (contentView.sortDescendingValue ? "\u25BC" : "\u25B2") : ""
                            color: contentView.sortKeyValue === 3 ? Theme.signal : Theme.textLow
                            font.pixelSize: Theme.fontMicro
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }

                // Modified column
                Item {
                    Layout.preferredWidth: 120
                    height: parent.height
                    MouseArea {
                        anchors.fill: parent
                        onClicked: contentView.sortBy(1)  // SortKey::Modified = 1
                    }
                    Row {
                        anchors.fill: parent
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: Theme.space1
                        Text {
                            text: "Modified"
                            color: contentView.sortKeyValue === 1 ? Theme.signal : Theme.textLow
                            font.pixelSize: Theme.fontMicro
                            font.bold: contentView.sortKeyValue === 1
                            horizontalAlignment: Text.AlignRight
                        }
                        Text {
                            text: contentView.sortKeyValue === 1 ? (contentView.sortDescendingValue ? "\u25BC" : "\u25B2") : ""
                            color: contentView.sortKeyValue === 1 ? Theme.signal : Theme.textLow
                            font.pixelSize: Theme.fontMicro
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }
                }
            }
        }

        // Manifest ListView with ManifestRow delegates
        ListView {
            id: manifestList
            anchors.top: manifestHeader.bottom
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            clip: true
            model: contentView.searchRecursive ? contentView.recursiveModel : contentView.searchProxy
            focus: contentView.manifest

            delegate: ManifestRow {
                objectName: "manifestCell"
                width: manifestList.width
                fileName: model.name !== undefined ? model.name : ""
                filePath: model.path !== undefined ? model.path : ""
                durationMs: model.duration !== undefined ? model.duration : -1
                sizeBytes: model.size !== undefined ? model.size : 0
                mtimeNs: model.mtime !== undefined ? model.mtime : 0
                isFolder: model.isFolder === true
                favorite: model.favorite === true
                watched: model.watched === true
                widthPx: model.width !== undefined ? model.width : 0
                heightPx: model.height !== undefined ? model.height : 0
                thumbnail: model.thumbnail !== undefined ? model.thumbnail : null
                thumbnailState: model.thumbnailState !== undefined ? model.thumbnailState : 0
                focused: index === contentView.selFocusIndex
                // Manifest rows are proxy rows: identityAt() resolves the source
                // row's identity through the manifest model (AUD-004).
                selected: selectionState
                          ? (contentView.selRevision >= 0
                             && selectionState.isSelected(contentView.identityAt(index)))
                          : (index === contentView.selectedIndex)
                onClicked: contentView.selectIndex(index)
                onDoubleClicked: contentView.activateIndex(index)
            }
        }

        // No-results overlay (Prompt 44)
        NoResults {
            id: noResultsOverlay
            anchors.fill: parent
            visible: contentView.manifest && manifestList.count === 0 && contentView.searchQuery.length > 0
            query: contentView.searchQuery
            onClearRequested: contentView.searchClearRequested()
        }

        // Result summary bar
        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: 24
            color: Theme.surface1
            visible: contentView.summaryText.length > 0

            Text {
                anchors.centerIn: parent
                text: contentView.summaryText
                color: Theme.textMid
                font.pixelSize: Theme.fontMicro
            }
        }
    }

    // --- Drag & Drop surface (REM-014 / D-27) --------------------------------
    signal dropRequested(var paths, string targetDir, bool isMove)

    // Supported media extensions — mirrors formats.hpp for drop validation.
    readonly property var supportedExtensions: ["mp4", "mkv", "webm", "avi", "mov", "m4v", "mpeg", "mpg", "ts", "m2ts", "mts", "ogv", "3gp", "flv", "wmv"]

    function handleDrop(urls, targetDir, isMove) {
        // Validate external drops: reject unsupported media (DESIGN §17.3 / REM-014).
        const validPaths = []
        for (let i = 0; i < (urls ? urls.length : 0); ++i) {
            const p = urls[i] ? (typeof urls[i] === "string" ? urls[i] : urls[i].toString()) : ""
            const filePath = p.indexOf("file://") === 0 ? p.substring(7) : p
            // Allow folders (no extension) and supported videos; reject unsupported files safely.
            const dot = filePath.lastIndexOf(".")
            const ext = (dot >= 0) ? filePath.substring(dot + 1).toLowerCase() : ""
            if (ext.length === 0 || supportedExtensions.indexOf(ext) >= 0) {
                validPaths.push(filePath)
            }
        }
        if (validPaths.length === 0) return  // unsupported drop, no-op
        dropRequested(validPaths, targetDir || "", isMove === true)
    }

    DropArea {
        id: contentDropArea
        anchors.fill: parent
        keys: ["text/uri-list"]
        onDropped: {
            if (!drop.hasUrls) return
            const urlStrs = drop.urls
            const paths = urlStrs.map(function(u) {
                const s = u.toString()
                return s.indexOf("file://") === 0 ? s.substring(7) : s
            })
            const isMove = !(drop.modifiers & Qt.ControlModifier)
            const target = window.browserController ? (window.browserController.currentPath || "") : ""
            handleDrop(paths, target, isMove)
        }
    }

    // Recursive search integration: react to SearchController scope changes
    Connections {
        target: window.searchController

        function onScopeChanged() {
            searchRecursive = window.searchController.scopeRecursive()
            recursiveModel = window.searchController.recursiveResultsModel()
        }
    }
}
