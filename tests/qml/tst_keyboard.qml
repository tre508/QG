// Central keyboard routing tests (REMEDIATION-012 / finding D-11).
//
// Verifies the shell's central shortcut map: every §13.2 binding forwards its
// action id to the injected KeyboardFramework; playback keys are live only while
// the Stage holds focus / Cinema; single-key shortcuts are suppressed while a text
// editor holds focus; and no two enabled shortcuts share a sequence (no duplicate
// activation, no QML "Ambiguous shortcut overload").

import QtQuick
import QtQuick.Controls
import QtTest
import qg

TestCase {
    name: "Keyboard"
    when: windowShown

    // Records every action id routed through the central map.
    component MockKeyboard: QtObject {
        property var calls: []
        function dispatch(id) { calls.push(id); return true }
        function last() { return calls.length > 0 ? calls[calls.length - 1] : "" }
    }

    component MockPlaybackController: QtObject {
        signal playbackStarted(string path)
        signal playbackEnded()
        signal endOfFolder()
        property bool cinemaActive: false
        property bool playing: false
        property var currentSession: null
        property int reelFilter: 0
        property string positionLabel: ""
        property bool canGoPrevious: false
        property bool canGoNext: false
        function play(path, identity) {}
        function stop() {}
        function enterCinema() { cinemaActive = true }
        function exitCinema() { cinemaActive = false }
        function toggleCinema() { cinemaActive ? exitCinema() : enterCinema() }
        function previous() {}
        function next() {}
        function setReelFilter(value) { reelFilter = value }
        function selectReelRow(row) {}
        function currentIdentity() { return "" }
    }

    component MockSession: QtObject {
        property int state: 2
        property string path: "/tmp/clip.mp4"
        property real position: 0
        property real duration: 120
        property real volume: 0.5
        property bool muted: false
        property bool seekable: true
        property string errorMessage: ""
    }

    component MockReelModel: QtObject {
        property int count: 0
    }

    Component { id: mainComponent; Main {} }

    ListModel {
        id: folderFixture
        ListElement { name: "clip.mp4"; path: "/tmp/clip.mp4"; isFolder: false }
    }

    // Walk visual children *and* non-visual objects (Shortcut lands in `data`).
    function findByName(item, name) {
        if (!item)
            return null
        if (item.objectName === name)
            return item
        const lists = [item.children, item.data, item.resources]
        for (let l = 0; l < lists.length; ++l) {
            const kids = lists[l]
            if (!kids)
                continue
            for (let i = 0; i < kids.length; ++i) {
                const found = findByName(kids[i], name)
                if (found)
                    return found
            }
        }
        return null
    }

    function collectShortcuts(item, out) {
        if (!item)
            return out
        if (item.sequence !== undefined && item.objectName !== "")
            out.push(item)
        const lists = [item.children, item.data, item.resources]
        for (let l = 0; l < lists.length; ++l) {
            const kids = lists[l]
            if (!kids)
                continue
            for (let i = 0; i < kids.length; ++i)
                collectShortcuts(kids[i], out)
        }
        return out
    }

    function seqOf(sc) {
        // QKeySequence → "Alt+Left"; fall back to the raw value's string form.
        return sc.sequence && sc.sequence.toString ? sc.sequence.toString()
                                                    : ("" + sc.sequence)
    }

    // The browse bindings that must be enabled with no session / no cinema.
    readonly property var browseBindings: [
        ["kbNavBack", "nav.back"], ["kbNavForward", "nav.forward"],
        ["kbNavParent", "nav.parent"], ["kbParentBackspace", "nav.parent"],
        ["kbRefresh", "nav.refresh"], ["kbHome", "nav.home"],
        ["kbLocation", "nav.location"], ["kbSearch", "view.search"],
        ["kbOrder", "view.order"], ["kbPlaces", "view.places"],
        ["kbDensity1", "view.density1"], ["kbDensity2", "view.density2"],
        ["kbDensity3", "view.density3"], ["kbOpen", "item.open"],
        ["kbRename", "item.rename"], ["kbTrash", "item.trash"],
        ["kbDeletePermanent", "item.deletePermanent"], ["kbFavorite", "item.favorite"],
        ["kbWatched", "item.watched"], ["kbReveal", "item.reveal"],
        ["kbProperties", "item.properties"]
    ]

    function test_browseShortcutsForwardActionIds() {
        const kb = mockKeyboardComponent.createObject(null)
        const pc = mockPcComponent.createObject(null)
        const reel = mockReelComponent.createObject(null)
        const shell = createTemporaryObject(mainComponent, null, {
            keyboard: kb, folderModel: folderFixture, playbackController: pc,
            reelModel: reel
        })
        verify(shell !== null)

        for (let i = 0; i < browseBindings.length; ++i) {
            const name = browseBindings[i][0]
            const id = browseBindings[i][1]
            const sc = findByName(shell, name)
            verify(sc !== null, "shortcut " + name + " present")
            compare(sc.enabled, true, name + " enabled while browsing")
            sc.activated()
            compare(kb.last(), id, name + " forwards " + id)
        }

        // Ctrl+R toggles the Reel (present because a reelModel is injected).
        const reelSc = findByName(shell, "reelToggleShortcut")
        verify(reelSc !== null)
        compare(reelSc.enabled, true)
        reelSc.activated()
        compare(kb.last(), "view.reel")
    }

    function test_playbackShortcutsAreLiveOnlyInStageContext() {
        const kb = mockKeyboardComponent.createObject(null)
        const pc = mockPcComponent.createObject(null)
        const shell = createTemporaryObject(mainComponent, null, {
            keyboard: kb, folderModel: folderFixture, playbackController: pc
        })
        verify(shell !== null)

        const space = findByName(shell, "kbPlayPauseSpace")
        verify(space !== null)
        // No session → Stage context is not live.
        compare(space.enabled, false, "playback keys idle with no session")

        // A live session plus Cinema (which focuses the Stage) enables them.
        pc.currentSession = sessionComponent.createObject(null)
        pc.enterCinema()
        wait(50)
        compare(shell.sessionActive, true)
        compare(space.enabled, true, "playback keys live in Cinema")
        space.activated()
        compare(kb.last(), "playback.playPause")

        // In Cinema the browse-only keys stand down.
        compare(findByName(shell, "kbSearch").enabled, false)
    }

    function test_textEditingSuppressesSingleKeyShortcuts() {
        const kb = mockKeyboardComponent.createObject(null)
        const pc = mockPcComponent.createObject(null)
        const shell = createTemporaryObject(mainComponent, null, {
            keyboard: kb, folderModel: folderFixture, playbackController: pc
        })
        verify(shell !== null)

        const rename = findByName(shell, "kbRename")
        compare(rename.enabled, true, "enabled before editing")

        const editor = Qt.createQmlObject(
            "import QtQuick.Controls; TextField { objectName: 'probeEditor' }",
            shell.contentItem)
        verify(editor !== null)
        editor.forceActiveFocus()
        wait(50)
        compare(shell.textEditing, true, "editor focus detected")
        compare(rename.enabled, false, "single-key shortcut suppressed while editing")
        compare(findByName(shell, "kbTrash").enabled, false)
        compare(findByName(shell, "kbOpen").enabled, false)

        editor.destroy()
        shell.contentItem.forceActiveFocus()
        wait(50)
        compare(rename.enabled, true, "re-enabled after editing")
    }

    function test_noDuplicateEnabledSequences() {
        const kb = mockKeyboardComponent.createObject(null)
        const pc = mockPcComponent.createObject(null)
        const reel = mockReelComponent.createObject(null)
        const shell = createTemporaryObject(mainComponent, null, {
            keyboard: kb, folderModel: folderFixture, playbackController: pc,
            reelModel: reel
        })
        verify(shell !== null)

        const all = collectShortcuts(shell, [])
        verify(all.length > 30, "central map is declared: " + all.length + " shortcuts")

        const seen = {}
        for (let i = 0; i < all.length; ++i) {
            const sc = all[i]
            if (sc.enabled !== true)
                continue
            const key = seqOf(sc)
            verify(seen[key] === undefined,
                   "duplicate enabled sequence '" + key + "' (" + sc.objectName + ")")
            seen[key] = sc.objectName
        }
    }

    Component { id: mockKeyboardComponent; MockKeyboard {} }
    Component { id: mockPcComponent; MockPlaybackController {} }
    Component { id: mockReelComponent; MockReelModel {} }
    Component { id: sessionComponent; MockSession {} }
}
