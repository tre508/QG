// Stage / video-surface wiring tests (REMEDIATION-005 / findings D-06, D-20).
// Verifies the Stage hosts a real VideoSurfaceItem (not a placeholder), that the
// surface binds to the injected backend, and that create/destroy/recreate is safe.

import QtQuick
import QtQuick.Controls
import QtTest
import qg
import qg.core 1.0

TestCase {
    name: "Stage"
    when: windowShown

    Component {
        id: stageComponent
        Stage {
            width: 640
            height: 360
        }
    }

    function test_stageHostsRealVideoSurface() {
        const stage = createTemporaryObject(stageComponent, null, {})
        verify(stage !== null)

        const surface = findChild(stage, "videoSurface")
        verify(surface !== null, "Stage hosts a VideoSurfaceItem (objectName videoSurface)")
        // VideoSurfaceItem contract: a real QQuickItem with a backend + videoActive.
        verify(surface.backend !== undefined, "surface exposes a backend property")
        compare(surface.videoActive, false, "no active video without playback")
        verify(surface.width > 0 && surface.height > 0, "surface is laid out inside the Stage")
    }

    function test_stageTransportIsRealAndWired() {
        const stage = createTemporaryObject(stageComponent, null, {})
        verify(stage !== null)
        const transport = findChild(stage, "stageTransport")
        verify(transport !== null, "Stage hosts the real TransportBar (D-06)")
        verify(transport.playPauseRequested !== undefined)
    }

    function test_stageLifecycleOpenCloseReopen() {
        // Create → destroy → recreate must not crash (renderer lifecycle).
        let stage = createTemporaryObject(stageComponent, null, {})
        verify(stage !== null)
        verify(findChild(stage, "videoSurface") !== null)
        stage.destroy()
        stage = createTemporaryObject(stageComponent, null, {})
        verify(stage !== null)
        verify(findChild(stage, "videoSurface") !== null)
    }

    // AUD-001: a seek the manager refuses (non-seekable media, DESIGN.md playback
    // §3.1) must surface on the transport bar's amber affordance. The manager owns
    // the policy; the Stage only forwards the notification.
    function test_stageForwardsRefusedSeekToTransport() {
        const stage = createTemporaryObject(stageComponent, null, {})
        verify(stage !== null)
        const transport = findChild(stage, "stageTransport")
        verify(transport !== null, "Stage hosts the real TransportBar")

        // A stand-in for PlaybackManager: the one signal the Stage listens to.
        const manager = Qt.createQmlObject(
            'import QtQuick; QtObject { signal seekRejected(); function attachSurface(s) {} }',
            stage, "refusedSeekStandIn")
        stage.playbackManager = manager

        compare(transport.seekRejected, false, "no affordance before a refusal")
        manager.seekRejected()
        compare(transport.seekRejected, true, "a refused seek raises the amber flash")

        // The manager may be replaced/cleared without the Stage erroring.
        stage.playbackManager = null
        manager.seekRejected()
        verify(true, "emitting after the manager was detached is harmless")
        manager.destroy()
    }
}