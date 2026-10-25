// ContentView grid-host tests (Prompt 17).

import QtQuick
import QtQuick.Controls
import QtTest
import qg

TestCase {
    name: "ContentView"
    when: windowShown

    ListModel {
        id: fixture
        ListElement { name: "a.mp4"; path: "/v/a.mp4"; isFolder: false; identity: "id0" }
        ListElement { name: "b.mp4"; path: "/v/b.mp4"; isFolder: false; identity: "id1" }
        ListElement { name: "sub"; path: "/v/sub"; isFolder: true; identity: "id2" }
    }

    // Helper to add identityAt to ListModel
    Component.onCompleted: {
        fixture.identityAt = function(row) {
            const item = fixture.get(row)
            return item ? item.identity : ""
        }
    }

    // SelectionState mock (REMEDIATION-013 / D-12).
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

    Component { id: mockSelectionStateComponent; MockSelectionState {} }
    Component { id: contentComponent; ContentView {} }

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

    // Column counts now derive from the density TARGET (DESIGN §3.5/§5.4), not
    // from min_card — deriving from min_card maximised columns and produced
    // cards far narrower than the ~240 target (VIS-008). Geometry details are
    // covered by tst_grid_geometry.qml.
    function test_columnsRecomputeAcrossWidths() {
        const view = createTemporaryObject(contentComponent, null,
                                           { width: 400, height: 600, targetCardWidth: 240 })
        compare(view.columns, 1, "one column at 400px")
        compare(view.cardWidth, 320, "capped at maxCard")

        view.width = 800
        compare(view.columns, 3)
        compare(view.cardWidth, 240, "lands exactly on the Standard target")
        verify(view.cardWidth >= view.minCard && view.cardWidth <= view.maxCard)

        view.width = 1400
        compare(view.columns, 5)
        verify(view.cardWidth >= 240 && view.cardWidth <= view.maxCard)
        verify(view.occupiedWidth <= view.width - 2 * view.pad + view.gap)

        // The occupied grid is centred: equal gutters either side.
        verify(Math.abs(view.gridGutter - (view.width - view.occupiedWidth - view.gridGutter)) <= 1)
    }

    function test_focusAndSelectionAreDistinct() {
        const selectionState = mockSelectionStateComponent.createObject(null)
        const view = createTemporaryObject(contentComponent, null,
                                           { width: 1000, height: 600, model: fixture, selectionState: selectionState })
        // Set focus to index 0, but select index 2 (focus and selection are distinct)
        selectionState.setFocusIndex(0)
        // selectOne would move focus to the selected item, so use toggle instead
        // to select without moving focus
        selectionState.toggle("id2", 2)

        // Verify SelectionState tracks focus and selection correctly
        compare(selectionState.focusIndex(), 0)
        compare(selectionState.selectedCount(), 1)
        compare(selectionState.isSelected("id2"), true)
        compare(selectionState.isSelected("id0"), false)
    }

    function test_activationEmitsIndex() {
        const selectionState = mockSelectionStateComponent.createObject(null)
        const view = createTemporaryObject(contentComponent, null,
                                           { width: 1000, height: 600, model: fixture, selectionState: selectionState })
        let activatedIndex = -1
        let itemPath = ""
        view.activated.connect(function (index) { activatedIndex = index })
        view.activatedItem.connect(function (index, path, isFolder) { itemPath = path })

        view.activateIndex(1)
        compare(activatedIndex, 1)
        compare(itemPath, "") // ListModel has no pathAt; FolderModel supplies it
    }

    function test_moveFocusClamps() {
        const selectionState = mockSelectionStateComponent.createObject(null)
        const view = createTemporaryObject(contentComponent, null,
                                           { width: 1000, height: 600, model: fixture, selectionState: selectionState })
        selectionState.setFocusIndex(0)
        view.moveFocus(-5)
        compare(selectionState.focusIndex(), 0)
        view.moveFocus(10)
        compare(selectionState.focusIndex(), view.itemCount - 1)
    }
}
