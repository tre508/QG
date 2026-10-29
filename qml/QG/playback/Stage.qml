import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import qg
import qg.core 1.0

// Stage container (DESIGN.md §3.4, §8.1, §9, §19.3) — inline playback surface.
// Hosts the real VideoSurfaceItem (mpv frames via the scene graph), the real
// TransportBar (D-21), the position chip, and — in Cinema — a fading top info
// bar. Presentation only: it reads the single session and forwards intent to
// the single PlaybackManager/PlaybackController (DESIGN.md §14.1).

Rectangle {
    id: stage
    objectName: "stage"
    property alias videoSurface: surfaceItem

    // Any key press in Cinema restores controls (DESIGN.md §25.7).
    Keys.onPressed: {
        if (stage.cinema) stage.wakeControls()
    }

    // Injected by Main.qml (the single owners).
    property var backend: null
    property var playbackManager: null
    property var playbackController: null
    property var session: null

    property bool playing: false
    // Display-ready *containing folder* of the live session (DESIGN.md §4.3): the
    // Cinema top overlay names the folder, not the media file — the fullscreen
    // wireframe reads "sunset_over_sea.mp4 · ~/Videos/2024/Summer trip · 7 / 42".
    // Injected by Main.qml, which owns the session and the browser controller.
    property string currentFolderPath: ""
    property string currentName: ""
    property int currentNumber: 0
    property int totalCount: 0
    property string errorText: ""

    // Cinema state (DESIGN.md §9): the Stage fills the window and shows the top
    // info bar. Owned by Main.qml, which derives it from PlaybackController.
    property bool cinema: false

    readonly property bool videoActive: surfaceItem.videoActive

    // Overlay visibility (§4.4). The top info bar and the bottom transport fade as
    // ONE unit, and this is that unit's state: Main.qml binds the pointer to it, so
    // the cursor is hidden exactly while the overlays are, instead of by a second
    // timer that could drift out of step with them.
    readonly property bool controlsVisible: transport.shown

    signal closeRequested()
    signal fullscreenRequested()

    // Register the surface with the single PlaybackManager so a load requested
    // before the surface has painted is completed once a render context exists.
    onPlaybackManagerChanged: if (playbackManager) playbackManager.attachSurface(surfaceItem)
    Component.onCompleted: if (playbackManager) playbackManager.attachSurface(surfaceItem)

    function wakeControls() {
        transport.wake()
    }

    // Leaving Cinema must restore the bar: it auto-hides there (§9.2), so its
    // opacity may already be 0 when the inline Stage (which keeps its controls
    // permanently visible, §2.1.6) comes back.
    onCinemaChanged: if (!cinema) transport.wake()

    width: parent ? parent.width : 800
    // Inline: 16:9 capped at ~60% of the available content height (DESIGN.md
    // §8.1). Cinema: fill the whole window and letterbox inside (DESIGN.md §9.1).
    // Bound relative to the parent only — no dependency on the video area's own
    // size — to avoid a binding loop.
    height: parent ? (cinema ? parent.height
                             : Math.min(parent.height * 0.6,
                                        parent.width * 9 / 16 + Theme.space4))
                   : 600

    color: Theme.ink
    clip: true

    // Appearance (playback §2.1.4): the video must appear as soon as the surface
    // is ready — no thumbnail→video cross-fade, fade-in or scale-in. The
    // bloom/collapse *states* are kept (they decide whether the Stage is shown at
    // all) but the property changes are applied instantly: the Behaviors that
    // tweened opacity/scale were the transition this requirement forbids.
    opacity: 0
    scale: 0.92

    states: [
        State {
            name: "bloomed"
            when: stage.playing || stage.currentName.length > 0
            PropertyChanges { target: stage; opacity: 1; scale: 1 }
        },
        State {
            name: "collapsed"
            when: !(stage.playing || stage.currentName.length > 0)
            PropertyChanges { target: stage; opacity: 0; scale: 0.92 }
        }
    ]

    // Position chip (DESIGN.md §8.2: the persistent "1:12 / 4:12" chip). Shown
    // bottom-right so it stays out of the way of the transport controls.
    Rectangle {
        id: positionChip
        objectName: "stagePositionChip"
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.rightMargin: Theme.space2
        anchors.bottomMargin: transport.height + Theme.space1
        radius: Theme.radiusButton
        color: Qt.rgba(5/255, 7/255, 13/255, 0.85)
        implicitWidth: positionText.implicitWidth + Theme.space3
        implicitHeight: 20
        visible: stage.totalCount > 0 && stage.errorText.length === 0

        Text {
            id: positionText
            anchors.centerIn: parent
            text: stage.currentNumber + " / " + stage.totalCount
            color: Theme.textHi
            font.pixelSize: Theme.fontMicro
        }
    }

    // Cinema top information overlay (DESIGN.md §4.3, fullscreen wireframe):
    // filename · containing folder · n / N, plus the explicit ✕ that closes
    // playback. It fades with the bottom transport and becomes inert with it, so
    // the ✕ can never be clicked while it is invisible.
    Rectangle {
        id: cinemaBar
        objectName: "cinemaBar"
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        // §4.3: larger targets than the inline Stage's.
        height: 56
        visible: stage.cinema
        enabled: transport.shown
        color: Qt.rgba(5/255, 7/255, 13/255, 0.6)
        opacity: stage.cinema ? transport.opacity : 0

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Theme.space4
            anchors.rightMargin: Theme.space2
            spacing: Theme.space2

            Text {
                objectName: "cinemaTitle"
                Layout.fillWidth: true
                elide: Text.ElideMiddle
                text: stage.currentName
                      + (stage.currentFolderPath.length > 0 ? "   \u00B7   " + stage.currentFolderPath : "")
                      + (stage.totalCount > 0
                         ? "   \u00B7   " + stage.currentNumber + " / " + stage.totalCount : "")
                color: Theme.textHi
                font.pixelSize: Theme.fontBody
            }

            ToolButton {
                objectName: "cinemaClose"
                implicitWidth: 40
                implicitHeight: 40
                contentItem: Item {
                    Icon { anchors.centerIn: parent; name: "close"; size: 20; color: Theme.textMid }
                }
                onClicked: stage.closeRequested()
                Accessible.name: "Exit fullscreen and stop"
            }
        }
    }

    // Video area: 16:9 max, letterboxed in --ink (DESIGN.md §9.1).
    // It is centred in the space *above* the transport, and its height budget
    // excludes the transport — otherwise a video that fills the 60 % cap gets its
    // bottom ~24 px covered by the controls (playback §2.1.6: controls below the
    // video, never on top of it).
    Item {
        id: stageVideoArea
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: -transport.height / 2
        readonly property real availWidth: Math.max(0, stage.width - Theme.space4)
        readonly property real availHeight: Math.max(0, stage.height - transport.height - Theme.space1 * 2)
        width: Math.min(availWidth, availHeight * 16 / 9)
        height: width * 9 / 16

        // Fallback backdrop (letterbox) shown until mpv presents a frame.
        Rectangle {
            anchors.fill: parent
            color: Theme.ink
            border.color: Theme.hairline
            border.width: 1

            Icon {
                anchors.centerIn: parent
                visible: !stage.videoActive
                name: stage.playing ? "play" : "pause"
                size: 32
                color: Theme.textLow
                opacity: 0.5
            }
        }

        // The single ownership surface: mpv frames enter the scene graph here.
        VideoSurfaceItem {
            id: surfaceItem
            objectName: "videoSurface"
            anchors.fill: parent
            backend: stage.backend
        }

        // Double-click toggles Cinema (DESIGN.md §9.2) and wakes the controls.
        MouseArea {
            objectName: "stageDoubleClickArea"
            anchors.fill: parent
            onDoubleClicked: stage.fullscreenRequested()
            // Clicking the Stage hands it keyboard focus so the playback
            // shortcuts (Space/K/J/L/…) become live (window.stageKeys).
            onClicked: { stage.forceActiveFocus(); stage.wakeControls() }
            // Any mouse movement also restores controls (DESIGN.md §25.7).
            onPositionChanged: stage.wakeControls()
        }



        // Playback failure state (DESIGN.md §8.1, §12): Next / Close.
        Column {
            objectName: "stageError"
            anchors.centerIn: parent
            spacing: Theme.space2
            visible: stage.errorText.length > 0

            Icon {
                anchors.horizontalCenter: parent.horizontalCenter
                name: "warn"
                size: 28
                color: Theme.warn
            }
            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                text: stage.errorText
                color: Theme.textMid
                font.pixelSize: Theme.fontBody
            }
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: Theme.space2
                Button {
                    text: qsTr("Next")
                    onClicked: if (stage.playbackController) stage.playbackController.next()
                }
                Button {
                    text: qsTr("Close")
                    onClicked: stage.closeRequested()
                }
            }
        }
    }

    // Transport (DESIGN.md §8.2–§8.3) wired to the single PlaybackManager.
    TransportBar {
        id: transport
        objectName: "stageTransport"
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right

        // Auto-hide is the Cinema overlay behaviour (§4.4/§9.2/§25.7). The inline
        // Stage always shows its controls (§2.1.6), so auto-hide is enabled only
        // here — and Cinema also sizes the controls up (§4.3).
        cinema: stage.cinema
        autoHideEnabled: stage.cinema

        playing: stage.session ? stage.session.state === 2 : false
        position: stage.session ? stage.session.position : 0
        duration: stage.session ? stage.session.duration : 0
        volume: stage.session ? stage.session.volume : 1.0
        muted: stage.session ? stage.session.muted : false
        seekable: stage.session ? stage.session.seekable : false
        bufferedEnd: stage.session ? stage.session.bufferedEnd : 0

        onPlayPauseRequested: if (stage.playbackManager) stage.playbackManager.togglePlayPause()
        onSeekBackRequested: if (stage.playbackManager) stage.playbackManager.seek(-10)
        onSeekForwardRequested: if (stage.playbackManager) stage.playbackManager.seek(10)
        onSeekToRequested: function (p) { if (stage.playbackManager) stage.playbackManager.seekAbsolute(p) }
        onVolumeRequested: function (v) { if (stage.playbackManager) stage.playbackManager.setVolume(v) }
        onMuteToggled: if (stage.playbackManager) stage.playbackManager.setMuted(!(stage.session && stage.session.muted))
        onPreviousRequested: if (stage.playbackController) stage.playbackController.previous()
        onNextRequested: if (stage.playbackController) stage.playbackController.next()
        onFullscreenRequested: stage.fullscreenRequested()
        onCloseRequested: stage.closeRequested()
    }

    // A refused seek (non-seekable media, DESIGN.md playback §3.1): the manager
    // has already restored the real position and owns the rule, so the bar only
    // shows its amber affordance. `target: null` (no manager injected) is a valid
    // no-op, so this stays safe when the Stage is used without a session.
    Connections {
        target: stage.playbackManager

        function onSeekRejected() {
            transport.flashSeekRejected()
        }
    }
}
