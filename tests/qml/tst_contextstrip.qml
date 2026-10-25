// ContextStrip tests (Prompt 21).

import QtQuick
import QtQuick.Controls
import QtTest
import qg

TestCase {
    name: "ContextStrip"
    when: windowShown

    Component { id: stripComponent; ContextStrip {} }

    function makeStrip(props) {
        const base = { width: 1300, availableWidth: 1300, folderName: "Summer trip",
                       videoCount: 42, folderCount: 3, orderLabel: "Name \u2191", density: 1 }
        return createTemporaryObject(stripComponent, null, Object.assign({}, base, props || {}))
    }

    function findByName(item, name) {
        if (!item) return null
        if (item.objectName === name) return item
        const kids = item.children
        if (!kids) return null
        for (let i = 0; i < kids.length; ++i) {
            const found = findByName(kids[i], name)
            if (found) return found
        }
        return null
    }

    function test_fullStateShowsCountsOrderDensity() {
        const strip = makeStrip()
        compare(strip.visible, true)
        compare(strip.compact, false)
        compare(findByName(strip, "contextFolderName").text, "Summer trip")
        compare(findByName(strip, "contextCounts").text, "42 videos, 3 folders")
        compare(findByName(strip, "contextCounts").visible, true)
        compare(findByName(strip, "contextOrder").text, "Name \u2191")
        compare(findByName(strip, "densitySelector").visible, true)
    }

    function test_collapseAndHideThresholds() {
        const strip = makeStrip({ availableWidth: 1000, width: 1000 })
        compare(strip.visible, true)
        compare(strip.compact, true)
        compare(findByName(strip, "contextCounts").visible, false)
        compare(findByName(strip, "densitySelector").visible, false)
        compare(findByName(strip, "contextOrder").visible, true)

        strip.availableWidth = 800
        compare(strip.visible, false)
    }

    function test_densitySelectionEmits() {
        const strip = makeStrip()
        let chosen = -1
        strip.densitySelected.connect(function (d) { chosen = d })
        findByName(strip, "densityOption").clicked()
        compare(chosen, 0)
    }
}
