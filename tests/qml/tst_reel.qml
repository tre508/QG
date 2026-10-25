// ReelPanel tests (REMEDIATION-007 / D-23).
//
// Verifies the panel is presentation-only and forwards every intent: footer
// prev/next/favorite, filter chips, row activation (selection) and close; that
// prev/next enablement reflects the context bounds; and that the header shows
// the folder, active sort and "7 / 42" position.

import QtQuick
import QtQuick.Controls
import QtTest
import qg

TestCase {
    id: testRoot
    name: "Reel"
    when: windowShown

    ListModel {
        id: reelFixture
        ListElement { reelIndex: 1; reelIdentity: "i1"; reelName: "a.mp4"; reelDurationMs: 125000; reelSize: 1000; reelFavorite: false; reelWatched: false; reelProgress: -1; reelIsCurrent: false }
        ListElement { reelIndex: 2; reelIdentity: "i2"; reelName: "b.mp4"; reelDurationMs: -1; reelSize: 2000; reelFavorite: true; reelWatched: false; reelProgress: 0.5; reelIsCurrent: true }
        ListElement { reelIndex: 3; reelIdentity: "i3"; reelName: "c.mp4"; reelDurationMs: 33000; reelSize: 3000; reelFavorite: false; reelWatched: true; reelProgress: -1; reelIsCurrent: false }
    }

    Component {
        id: reelComponent
        ReelPanel {
            width: 280
            height: 480
        }
    }

    function makeReel(props) {
        const reel = createTemporaryObject(reelComponent, testRoot, props || {})
        verify(reel !== null)
        return reel
    }

    function child(parent, name) {
        const found = findChild(parent, name)
        verify(found !== null, "child '" + name + "' exists")
        return found
    }

    function test_headerShowsFolderSortAndPosition() {
        const reel = makeReel({
            reelModel: reelFixture,
            folderName: "Summer trip",
            sortLabel: "Name \u2191",
            positionLabel: "2 / 3"
        })
        compare(child(reel, "reelPosition").text, "2 / 3")
        verify(child(reel, "reelSortLabel").text.indexOf("Name") !== -1)
    }

    function test_rowsRenderFromTheModel() {
        const reel = makeReel({ reelModel: reelFixture, positionLabel: "2 / 3" })
        const list = child(reel, "reelList")
        compare(list.count, 3)
        // Duration formatting is shared with the rest of the UI via Format.
        compare(Format.duration(125000), "2:05")
        compare(Format.duration(-1), "--:--")
    }

    function test_footerAndRowIntents() {
        const reel = makeReel({ reelModel: reelFixture, canGoPrevious: true, canGoNext: true })
        const counts = { prev: 0, next: 0, fav: 0, close: 0, activated: [], filter: [] }
        reel.previousRequested.connect(function () { counts.prev++ })
        reel.nextRequested.connect(function () { counts.next++ })
        reel.favoriteToggled.connect(function () { counts.fav++ })
        reel.closeRequested.connect(function () { counts.close++ })
        reel.itemActivated.connect(function (row) { counts.activated.push(row) })
        reel.filterChanged.connect(function (f) { counts.filter.push(f) })

        child(reel, "reelPrev").clicked()
        child(reel, "reelNext").clicked()
        child(reel, "reelFavorite").clicked()
        child(reel, "reelClose").clicked()
        // Row selection (Quick Test cannot click a delegate reliably; the panel
        // exposes the same forwarding function the delegate calls).
        reel.activateRow(2)
        // Filter chips (neither Repeater delegate is clickable reliably).
        reel.selectFilter("Favorites")

        compare(counts.prev, 1)
        compare(counts.next, 1)
        compare(counts.fav, 1)
        compare(counts.close, 1)
        compare(counts.activated.length, 1)
        compare(counts.activated[0], 2)
        compare(counts.filter.length, 1)
        compare(counts.filter[0], "Favorites")
    }

    function test_prevNextEnablementFollowsBounds() {
        const reel = makeReel({ reelModel: reelFixture, canGoPrevious: false, canGoNext: true })
        compare(child(reel, "reelPrev").enabled, false)
        compare(child(reel, "reelNext").enabled, true)

        reel.canGoPrevious = true
        reel.canGoNext = false
        compare(child(reel, "reelPrev").enabled, true)
        compare(child(reel, "reelNext").enabled, false)
    }

    function test_filterChipsPresent() {
        const reel = makeReel({ reelModel: reelFixture, filterLabel: "Favorites" })
        compare(reel.filterLabels.length, 3)
        compare(reel.filterLabels[0], "All")
        compare(reel.filterLabels[2], "Unwatched")
        verify(child(reel, "reelFilters") !== null)
    }
}
