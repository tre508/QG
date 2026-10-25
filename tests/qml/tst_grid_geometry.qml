// Standard-density grid geometry (FIX-UI-004, DESIGN.md §3.5 / §5.4).
//
// Guards the two reported defects:
//   VIS-008 — cards far narrower than the documented ~240 target (the column
//             count used to be derived from min_card, maximising columns).
//   VIS-009 — the grid pinned to the left with a dead band on the right (a
//             non-zero rightMargin shrank the flow below columns*cellWidth, so
//             GridView laid out one column fewer than the geometry assumed).
//
// Every assertion below is a measured property of the live QGrid layout, not a
// reimplementation of the formula.

import QtQuick
import QtQuick.Controls
import QtTest
import qg

TestCase {
    name: "GridGeometry"
    when: windowShown

    // Shell-visible workspace widths for the required window widths, following
    // the DESIGN §3.5 sidebar modes (panel 240 / rail 56 / hidden).
    readonly property var cases: [
        { win: 1833, ws: 1593, target: 240, density: ContentView.Standard },
        { win: 1500, ws: 1260, target: 240, density: ContentView.Standard },
        { win: 1150, ws: 1094, target: 220, density: ContentView.Standard },
        { win: 1100, ws: 1044, target: 220, density: ContentView.Standard },
        { win: 900, ws: 844, target: 200, density: ContentView.Standard },
        { win: 820, ws: 764, target: 190, density: ContentView.Standard },
        { win: 700, ws: 700, target: 190, density: ContentView.Standard }
    ]

    ListModel {
        id: fixture
        ListElement { name: "a.mp4"; path: "/v/a.mp4"; isFolder: false; size: 100; mtime: 0 }
        ListElement { name: "b.mp4"; path: "/v/b.mp4"; isFolder: false; size: 200; mtime: 0 }
        ListElement { name: "c.mp4"; path: "/v/c.mp4"; isFolder: false; size: 300; mtime: 0 }
        ListElement { name: "d.mp4"; path: "/v/d.mp4"; isFolder: false; size: 300; mtime: 0 }
        ListElement { name: "e.mp4"; path: "/v/e.mp4"; isFolder: false; size: 300; mtime: 0 }
        ListElement { name: "f.mp4"; path: "/v/f.mp4"; isFolder: false; size: 300; mtime: 0 }
        ListElement { name: "g.mp4"; path: "/v/g.mp4"; isFolder: false; size: 300; mtime: 0 }
        ListElement { name: "h.mp4"; path: "/v/h.mp4"; isFolder: false; size: 300; mtime: 0 }
        ListElement { name: "i.mp4"; path: "/v/i.mp4"; isFolder: false; size: 300; mtime: 0 }
        ListElement { name: "j.mp4"; path: "/v/j.mp4"; isFolder: false; size: 300; mtime: 0 }
        ListElement { name: "k.mp4"; path: "/v/k.mp4"; isFolder: false; size: 300; mtime: 0 }
        ListElement { name: "l.mp4"; path: "/v/l.mp4"; isFolder: false; size: 300; mtime: 0 }
        ListElement { name: "m.mp4"; path: "/v/m.mp4"; isFolder: false; size: 300; mtime: 0 }
        ListElement { name: "n.mp4"; path: "/v/n.mp4"; isFolder: false; size: 300; mtime: 0 }
    }

    ListModel { id: bigFixture }

    Component { id: contentComponent; ContentView {} }

    // Minimal SelectionState stand-in (the production path in ContentView).
    Component {
        id: selectionStateComponent
        QtObject {
            property int focus: -1
            function focusIndex() { return focus }
            function setFocusIndex(i) { focus = i }
            function isSelected(identity) { return false }
            function clear() { }
            function toggle(identity, index) { }
        }
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

    function makeView(width, density, target) {
        return createTemporaryObject(contentComponent, null,
                                     { width: width, height: 900, model: fixture,
                                       density: density === undefined ? ContentView.Standard : density,
                                       targetCardWidth: target === undefined ? 240 : target })
    }

    // Cards actually laid out on the first row, measured in viewport space.
    function rowZero(view) {
        const grid = findByName(view, "contentGrid")
        const first = grid.itemAtIndex(0)
        if (!first)
            return { count: 0, left: -1, right: -1, lastRight: -1 }
        const y0 = first.y
        const left = first.mapToItem(grid, 0, 0).x
        let count = 1
        let lastRight = left + first.width
        for (let i = 1; i < grid.count; ++i) {
            const it = grid.itemAtIndex(i)
            if (!it || Math.abs(it.y - y0) >= 1)
                break
            ++count
            lastRight = Math.max(lastRight, it.mapToItem(grid, 0, 0).x + it.width)
        }
        return { count: count, left: left, right: grid.width - lastRight,
                 lastRight: lastRight, gridWidth: grid.width, grid: grid }
    }

    // VIS-008: at the reference wide viewport cards approach the 240 target.
    function test_referenceViewportCardWidthApproachesTarget() {
        const c = cases[0] // 1833px window → 1593px workspace, target 240
        const view = makeView(c.ws, c.density, c.target)
        compare(view.columns, 6, "6 columns at the reference viewport (§5.4 3–6)")
        verify(view.cardWidth >= 232 && view.cardWidth <= 256,
               "card width approaches 240 (got " + view.cardWidth + ")")
        verify(view.cardWidth <= view.maxCard, "never above the 320 ceiling")
        verify(view.cardWidth >= view.minCard, "never below the 168 floor")
    }

    // VIS-009: the nominal column count must match what is actually laid out, and
    // the leftover width must be split evenly between the two gutters.
    function test_columnsMatchLayoutAndGuttersAreBalanced() {
        for (let i = 0; i < cases.length; ++i) {
            const c = cases[i]
            const view = makeView(c.ws, c.density, c.target)
            wait(30)
            const row = rowZero(view)
            compare(row.count, view.columns,
                    "laid-out columns == computed columns at " + c.win + "px")
            verify(Math.abs(row.left - row.right) <= 1.5,
                   "gutters balanced at " + c.win + "px (left " + row.left.toFixed(1)
                   + ", right " + row.right.toFixed(1) + ")")
            verify(row.left >= view.pad - 0.5,
                   "left gutter respects the content padding at " + c.win + "px")
            view.destroy()
        }
    }

    // No card may cross the right edge of the viewport (no clipping / overflow).
    function test_noCardOverflowsTheViewport() {
        for (let i = 0; i < cases.length; ++i) {
            const c = cases[i]
            const view = makeView(c.ws, c.density, c.target)
            wait(30)
            const row = rowZero(view)
            verify(row.lastRight <= row.gridWidth + 0.5,
                   "last card inside the viewport at " + c.win + "px")
            verify(row.count * view.cellHeight > 0, "cell height positive")
            view.destroy()
        }
    }

    // No unintended horizontal scrolling. (GridView reports contentWidth -1 and a
    // negative contentX because leftMargin shifts the flow origin, so atXEnd is
    // not meaningful; the flow displacement and the card edges are.)
    function test_noHorizontalDisplacement() {
        const view = makeView(cases[0].ws, cases[0].density, cases[0].target)
        const grid = findByName(view, "contentGrid")
        wait(50)
        verify(grid.contentX <= 0, "grid is not scrolled to the right")
        compare(grid.contentX, -grid.leftMargin,
                "flow origin sits exactly at the centring gutter")
        const row = rowZero(view)
        verify(row.lastRight <= row.gridWidth + 0.5, "flow fits inside the viewport")
    }

    // Thumbnail region keeps 16:9; the metadata band is a fixed height so text
    // length can never change the row height.
    function test_thumbnailAspectAndStableMetadataBand() {
        for (let i = 0; i < cases.length; ++i) {
            const c = cases[i]
            const v = makeView(c.ws, c.density, c.target)
            compare(v.thumbHeight, Math.round(v.cardWidth * 9 / 16),
                    "thumbnail is 16:9 at " + c.win + "px")
            compare(v.cellHeight - v.thumbHeight, v.metaBandHeight,
                    "metadata band constant at " + c.win + "px")
            compare(v.metaBandHeight, 44, "documented band height")
            v.destroy()
        }
    }

    // Degenerate widths: at least one column, never a zero/negative card, never
    // wider than the viewport.
    function test_degenerateWidthsStayValid() {
        const widths = [0, 1, 40, 100, 168, 200, 260]
        for (let i = 0; i < widths.length; ++i) {
            const view = makeView(widths[i], ContentView.Standard, 240)
            verify(view.columns >= 1, "at least one column at width " + widths[i])
            verify(view.cardWidth >= 1, "positive card width at width " + widths[i])
            verify(view.cardWidth <= Math.max(1, view.availableWidth),
                   "card never exceeds usable width at width " + widths[i])
            verify(view.thumbHeight >= 0 && view.cellHeight > 0, "valid heights")
            view.destroy()
        }
    }

    // The 320 ceiling is honoured even when the target would allow wider cards;
    // the leftover then becomes symmetric gutters (never stretched thumbnails).
    function test_ceilingClampsAndCenters() {
        const view = makeView(2400, ContentView.Standard, 240)
        wait(30)
        verify(view.cardWidth <= view.maxCard, "card capped at maxCard")
        const row = rowZero(view)
        compare(row.count, view.columns, "columns match layout at 2400px")
        verify(Math.abs(row.left - row.right) <= 1.5,
               "leftover width centred (left " + row.left.toFixed(1)
               + ", right " + row.right.toFixed(1) + ")")
        verify(row.left > view.pad, "centring actually engaged (gutter wider than padding)")
    }

    // The shell's §3.5 target is an input: a smaller target packs more columns.
    function test_targetDrivesColumnCount() {
        const wide = makeView(1593, ContentView.Standard, 240)
        const wideCols = wide.columns
        wide.destroy()
        const dense = makeView(1593, ContentView.Standard, 190)
        const denseCols = dense.columns
        dense.destroy()
        verify(denseCols > wideCols,
               "smaller target yields more columns (" + denseCols + " > " + wideCols + ")")
    }

    // Gallery keeps its own larger cards and fewer columns.
    function test_galleryStaysLargerAndSparser() {
        const std = makeView(1593, ContentView.Standard, 240)
        const stdCols = std.columns
        const stdW = std.cardWidth
        std.destroy()
        const gal = makeView(1593, ContentView.Gallery, 240)
        wait(30)
        verify(gal.columns < stdCols, "gallery has fewer columns")
        verify(gal.cardWidth > stdW, "gallery cards are larger")
        verify(gal.cardWidth <= gal.maxCard, "gallery respects its ceiling")
        const row = rowZero(gal)
        compare(row.count, gal.columns, "gallery columns match layout")
        verify(Math.abs(row.left - row.right) <= 1.5, "gallery gutters balanced")
    }

    // Geometry must not depend on focus/selection: resizing keeps the focused
    // row valid and its card reachable.
    function test_resizeKeepsFocusIndexValid() {
        const sel = selectionStateComponent.createObject(null)
        verify(sel !== null)
        const view = createTemporaryObject(contentComponent, null,
                                           { width: 1593, height: 900, model: fixture,
                                             density: ContentView.Standard, targetCardWidth: 240,
                                             selectionState: sel })
        const grid = findByName(view, "contentGrid")
        wait(30)
        sel.setFocusIndex(9)
        const before = view.columns
        view.width = 1260
        wait(30)
        verify(view.columns !== before, "column count changed with the width")
        compare(sel.focusIndex(), 9, "focus index unchanged by reflow")
        grid.positionViewAtIndex(9, GridView.Contain)
        wait(30)
        verify(grid.itemAtIndex(9) !== null, "focused card remains reachable after reflow")
        sel.destroy()
    }

    // Virtualization: a large model must not instantiate a delegate per row.
    function test_largeModelRemainsVirtualized() {
        bigFixture.clear()
        for (let i = 0; i < 1000; ++i) {
            bigFixture.append({ name: "v" + i + ".mp4", path: "/v/v" + i + ".mp4",
                                isFolder: false, size: 100, mtime: 0 })
        }
        const view = createTemporaryObject(contentComponent, null,
                                           { width: 1593, height: 900, model: bigFixture,
                                             density: ContentView.Standard, targetCardWidth: 240 })
        const grid = findByName(view, "contentGrid")
        wait(80)
        compare(grid.count, 1000, "model fully available to the view")
        const instantiated = grid.contentItem.children.length
        verify(instantiated > 0, "some delegates exist")
        verify(instantiated < 200,
               "delegates are virtualized (" + instantiated + " created for 1000 rows)")
    }
}
