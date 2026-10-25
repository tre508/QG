// Virtualization / drift checks for the Visual Browser Checkpoint (CP-C).

import QtQuick
import QtQuick.Controls
import QtTest
import qg

TestCase {
    name: "Virtualization"
    when: windowShown

    Component { id: contentComponent; ContentView {} }
    Component { id: listModelComponent; ListModel {} }

    function collectByName(item, name, out) {
        if (!item) return out
        if (item.objectName === name) out.push(item)
        const kids = item.children
        if (!kids) return out
        for (let i = 0; i < kids.length; ++i)
            collectByName(kids[i], name, out)
        return out
    }

    function test_largeModelDoesNotInstantiatePerRow() {
        const model = listModelComponent.createObject(null)
        const rows = 2000
        for (let i = 0; i < rows; ++i)
            model.append({ name: "clip_" + i + ".mp4", path: "/v/" + i, isFolder: false })

        const view = createTemporaryObject(contentComponent, null,
                                           { width: 1000, height: 600, model: model })
        compare(view.itemCount, rows)

        const cells = collectByName(view, "contentCell", [])
        verify(cells.length > 0)
        // Virtualized: far fewer delegates than rows (never one per row).
        verify(cells.length < 200)
    }

    function test_cardBounds700To1920() {
        const view = createTemporaryObject(contentComponent, null,
                                           { width: 700, height: 600, model: null })
        let widths = [700, 1100, 1500, 1920]
        for (let i = 0; i < widths.length; ++i) {
            view.width = widths[i]
            verify(view.columns >= 1)
            verify(view.cardWidth <= view.maxCard)
            verify(view.cardWidth >= view.minCard)
        }
    }
}
