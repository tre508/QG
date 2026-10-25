// TransportBar control tests (REMEDIATION-006 / D-21, PLAY-003, PLAY-007).
//
// Verifies that every control forwards the right intent, that the seek gesture
// is optimistic + coalesced to a single commit, that a non-seekable stream
// rejects the seek (amber flash, snap back), and that the bar reflects playback
// state (glyph, timeline/time, volume).

import QtQuick
import QtQuick.Controls
import QtTest
import qg

TestCase {
    id: testRoot
    name: "TransportBar"
    when: windowShown

    Component {
        id: barComponent
        TransportBar {
            width: 720
        }
    }

    function makeBar(props) {
        // Parent under the test root so the bar lives in the (shown) test
        // window — mouse gestures need a real window.
        const bar = createTemporaryObject(barComponent, testRoot, props || {})
        verify(bar !== null)
        return bar
    }

    function child(parent, name) {
        const found = findChild(parent, name)
        verify(found !== null, "child '" + name + "' exists")
        return found
    }

    function test_controlsForwardIntent() {
        const bar = makeBar({})
        const counts = { play: 0, prev: 0, next: 0, back: 0, forward: 0,
                         mute: 0, fs: 0, close: 0, volume: [] }
        bar.playPauseRequested.connect(function () { counts.play++ })
        bar.previousRequested.connect(function () { counts.prev++ })
        bar.nextRequested.connect(function () { counts.next++ })
        bar.seekBackRequested.connect(function () { counts.back++ })
        bar.seekForwardRequested.connect(function () { counts.forward++ })
        bar.muteToggled.connect(function () { counts.mute++ })
        bar.fullscreenRequested.connect(function () { counts.fs++ })
        bar.closeRequested.connect(function () { counts.close++ })
        bar.volumeRequested.connect(function (v) { counts.volume.push(v) })

        child(bar, "transportPlayPause").clicked()
        child(bar, "transportPrev").clicked()
        child(bar, "transportNext").clicked()
        child(bar, "transportSeekBack").clicked()
        child(bar, "transportSeekForward").clicked()
        child(bar, "transportMute").clicked()
        child(bar, "transportFullscreen").clicked()
        child(bar, "transportClose").clicked()

        compare(counts.play, 1)
        compare(counts.prev, 1)
        compare(counts.next, 1)
        compare(counts.back, 1)
        compare(counts.forward, 1)
        compare(counts.mute, 1)
        compare(counts.fs, 1)
        compare(counts.close, 1)

        // Volume slider emits the canonical 0..1 fraction.
        const vol = child(bar, "transportVolume")
        vol.value = 0.5
        vol.moved()
        compare(counts.volume.length, 1)
        fuzzyCompare(counts.volume[0], 0.5, 0.001)
    }

    function test_seekIsOptimisticAndCoalesced() {
        const bar = makeBar({ duration: 100, position: 0, seekable: true })
        let seeks = []
        bar.seekToRequested.connect(function (p) { seeks.push(p) })

        const timeline = child(bar, "transportTimeline")
        verify(timeline.width > 10)

        // Pointer down, several moves, pointer up — one coalesced seek.
        bar.beginScrub(10)
        compare(bar.scrubbing, true)

        // Real slider path: a move updates the optimistic value.
        timeline.value = 40
        timeline.moved()
        timeline.value = 60
        timeline.moved()

        compare(seeks.length, 0, "no seek while the gesture is in flight")
        fuzzyCompare(bar.displayPosition, 60, 0.001, "scrubber is optimistic")

        bar.commitScrub()
        compare(bar.scrubbing, false)
        compare(seeks.length, 1, "one coalesced seek per gesture")
        fuzzyCompare(seeks[0], 60, 0.001)
    }

    function test_nonSeekableStreamRejectsSeek() {
        const bar = makeBar({ duration: 100, position: 0, seekable: false })
        let seeks = []
        bar.seekToRequested.connect(function (p) { seeks.push(p) })

        const timeline = child(bar, "transportTimeline")
        timeline.value = 70
        timeline.moved()

        compare(seeks.length, 0, "no seek emitted for a non-seekable stream")
        compare(bar.seekRejected, true, "amber flash raised")

        bar.beginScrub(0)
        bar.commitScrub()
        compare(seeks.length, 0, "release still emits nothing when not seekable")
        // Snaps back: the tracked position is unchanged.
        fuzzyCompare(timeline.value, 0, 0.001)
    }

    function test_reflectsPlaybackState() {
        const bar = makeBar({ playing: false, duration: 125, position: 72, volume: 0.4 })
        const play = child(bar, "transportPlayPause")
        compare(play.contentItem.children[0].name, "play", "play icon when paused")

        bar.playing = true
        compare(play.contentItem.children[0].name, "pause", "pause icon when playing")

        const vol = child(bar, "transportVolume")
        fuzzyCompare(vol.value, 0.4, 0.001)

        const time = child(bar, "transportTime")
        compare(time.text, "1:12 / 2:05")
    }
}
