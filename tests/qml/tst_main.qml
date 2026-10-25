// QML load tests for the QG shell (Prompt 03).
//
// Verifies the `qg` module resolves, Theme tokens expose the approved values,
// and Main.qml instantiates with the six region placeholders.

import QtQuick
import QtQuick.Controls
import QtTest
import qg

TestCase {
    name: "MainShell"
    when: windowShown

    Component {
        id: mainComponent
        Main {}
    }

    // QObject::findChild() is not exposed to QML, so walk the visual tree.
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

    function test_themeTokensResolve() {
        compare(String(Theme.ink).toUpperCase(), "#05070D")
        compare(String(Theme.surface0).toUpperCase(), "#080C15")
        compare(String(Theme.signal).toUpperCase(), "#4DA3FF")
        compare(String(Theme.focus).toUpperCase(), "#38E1E1")
        compare(String(Theme.play).toUpperCase(), "#3DE0A0")
        compare(String(Theme.favorite).toUpperCase(), "#FF5CC8")
        compare(String(Theme.watched).toUpperCase(), "#9B7BFF")
        compare(String(Theme.danger).toUpperCase(), "#FF5D6C")
        compare(String(Theme.warn).toUpperCase(), "#FFC65C")

        compare(Theme.radiusCard, 10)
        compare(Theme.radiusStage, 14)
        compare(Theme.space4, 16)
        compare(Theme.space8, 48)

        compare(Theme.motionMicro, 120)
        compare(Theme.motionBloom, 220)
        compare(Theme.motionCinema, 260)
    }

    function test_mainInstantiatesWithRegions() {
        const object = createTemporaryObject(mainComponent, null)
        verify(object !== null)

        // Six region placeholders (DESIGN.md §3.1; identity is the window title).
        const root = object.contentItem ? object.contentItem : object
        verify(findByName(root, "sidebarRegion") !== null)
        verify(findByName(root, "pathBarRegion") !== null)
        verify(findByName(root, "contextStripRegion") !== null)
        verify(findByName(root, "contentSurfaceRegion") !== null)
        verify(findByName(root, "reelRegion") !== null)
        verify(findByName(root, "statusStripRegion") !== null)
    }
}
