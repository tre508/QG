// StatusStrip tests (Prompt 22).

import QtQuick
import QtQuick.Controls
import QtTest
import qg

TestCase {
    name: "StatusStrip"
    when: windowShown

    Component { id: stripComponent; StatusStrip {} }

    function makeStrip(props) {
        const base = { width: 1000, height: 28, selectedCount: 1,
                       selectedBytes: 2254857830, volumeFreeBytes: 236223201280,
                       volumeLabel: "SSD", busyCount: 1,
                       tasks: [{ label: "Thumbnailing", count: 3 }], transientText: "" }
        return createTemporaryObject(stripComponent, null, Object.assign({}, base, props || {}))
    }

    function findBy(item, name) {
        if (!item) return null
        if (item.objectName === name) return item
        const kids = item.children
        if (!kids) return null
        for (let i = 0; i < kids.length; ++i) {
            const found = findBy(kids[i], name)
            if (found) return found
        }
        return null
    }

    function test_selectionSummary() {
        const strip = makeStrip()
        const sel = findBy(strip, "statusSelection")
        compare(sel.visible, true)
        compare(sel.text, "1 selected \u00B7 2.1 GB")

        strip.selectedCount = 0
        compare(sel.text, "No selection")
    }

    function test_freeSpaceKnownAndUnknown() {
        const strip = makeStrip()
        const free = findBy(strip, "statusFreeSpace")
        compare(free.visible, true)
        verify(free.text.indexOf("Free") === 0)
        verify(free.text.indexOf("SSD") !== -1)

        // Unknown free space must render no claim.
        strip.volumeFreeBytes = -1
        compare(free.visible, false)
    }

    function test_activityClusterAndPopover() {
        const strip = makeStrip()
        compare(findBy(strip, "statusActivity").visible, true)
        compare(findBy(strip, "activityCount").text, "1")

        const popup = strip.activityPopupRef
        verify(popup !== null)
        strip.openActivity() // must not throw; popup needs a window to become visible
        verify(popup.contentItem !== null)

        strip.busyCount = 0
        compare(findBy(strip, "statusActivity").visible, false)
    }

    function test_transientText() {
        const strip = makeStrip({ transientText: "Renamed clip.mp4" })
        const t = findBy(strip, "statusTransient")
        compare(t.visible, true)
        compare(t.text, "Renamed clip.mp4")
    }
}
