// FolderCard delegate tests (Prompt 19).

import QtQuick
import QtQuick.Controls
import QtTest
import qg

TestCase {
    name: "FolderCard"
    when: windowShown

    Component { id: cardComponent; FolderCard {} }

    function makeCard(props) {
        const base = { width: 280, height: 220, folderName: "Summer trip",
                       videoCount: 12, folderCount: 3,
                       mosaicSources: ["a", "b", "c", "d"] }
        return createTemporaryObject(cardComponent, null, Object.assign({}, base, props || {}))
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

    function collectByName(item, name, out) {
        if (!item) return out
        if (item.objectName === name) out.push(item)
        const kids = item.children
        if (!kids) return out
        for (let i = 0; i < kids.length; ++i)
            collectByName(kids[i], name, out)
        return out
    }

    function test_mosaicCountChipAndMeta() {
        const card = makeCard()
        verify(card.hasMosaic)
        compare(collectByName(card, "mosaicCell", []).length, 4)
        compare(findByName(card, "folderGlyph").visible, false)
        compare(findByName(card, "countChipText").text, "12v \u00B7 3")
        compare(findByName(card, "folderMeta").text, "12 videos \u00B7 3 subfolders")
        // No duration chip on folders.
        compare(findByName(card, "durationChip"), null)
    }

    function test_glyphFallbackWhenNoThumbnails() {
        const card = makeCard({ mosaicSources: [], folderCount: 0, videoCount: 5 })
        compare(card.hasMosaic, false)
        compare(findByName(card, "folderGlyph").visible, true)
        compare(findByName(card, "countChipText").text, "5v")
        compare(findByName(card, "folderMeta").text, "5 videos \u00B7 0 subfolders")
    }

    function test_hoverFocusSelection() {
        const card = makeCard({ hovered: true, focused: true, selected: true })
        compare(findByName(card, "hoverChevron").visible, true)
        compare(findByName(card, "focusRing").visible, true)
        compare(findByName(card, "selectionBorder").visible, true)
    }
}
