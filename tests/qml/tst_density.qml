// Density-mode tests (Prompt 20).

import QtQuick
import QtQuick.Controls
import QtTest
import qg

TestCase {
    name: "Density"
    when: windowShown

    ListModel {
        id: fixture
        ListElement { name: "a.mp4"; path: "/v/a.mp4"; isFolder: false; size: 100; mtime: 0 }
        ListElement { name: "b.mp4"; path: "/v/b.mp4"; isFolder: false; size: 200; mtime: 0 }
        ListElement { name: "sub"; path: "/v/sub"; isFolder: true }
    }

    Component { id: contentComponent; ContentView {} }

    function test_densitySwitchesPresentationPreservingModel() {
        const view = createTemporaryObject(contentComponent, null,
                                           { width: 800, height: 600, model: fixture })
        // Default Standard -> grid.
        compare(view.density, ContentView.Standard)
        verify(view.itemCount === 3)

        const grid = findByName(view, "contentGrid")
        const compact = findByName(view, "compactList")
        verify(grid.visible)
        verify(!compact.visible)

        view.density = ContentView.Compact
        verify(compact.visible)
        verify(!grid.visible)
        compare(view.itemCount, 3) // same canonical model

        view.density = ContentView.Gallery
        verify(grid.visible)
        compare(view.itemCount, 3)
    }

    function test_galleryUsesLargerCardsFewerColumns() {
        const view = createTemporaryObject(contentComponent, null,
                                           { width: 800, height: 600, model: fixture })
        view.density = ContentView.Standard
        const standardColumns = view.columns
        view.density = ContentView.Gallery
        const galleryColumns = view.columns
        verify(galleryColumns < standardColumns)
        verify(view.cardWidth >= 240 && view.cardWidth <= 360)
    }

    function test_compactRowsRender() {
        const view = createTemporaryObject(contentComponent, null,
                                           { width: 800, height: 600, model: fixture,
                                             density: ContentView.Compact })
        const compact = findByName(view, "compactList")
        compare(compact.count, 3)
        verify(findByName(view, "compactName") !== null)
        verify(findByName(view, "compactDuration") !== null)
        verify(findByName(view, "compactSize") !== null)
        verify(findByName(view, "compactDate") !== null)
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
}
