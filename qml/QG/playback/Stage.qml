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
    property string currentPath: ""
    property string currentName: ""
    property int currentNumber: 0
    property int totalCount: 0
    property string errorText: ""

    // Cinema state (DESIGN.md §9): the Stage fills the window and shows the top
    // info bar. Owned by Main.qml, which derives it from PlaybackController.
    property bool cinema: false

    readonly property bool videoActive: surfaceItem.videoActive

    signal closeRequested()
    signal fullscreenRequested()

    // Register the surface with the single PlaybackManager so a load requested
    // before the surface has painted is completed once a render context exists.
    onPlaybackManagerChanged: if (playbackManager) playbackManager.attachSurface(surfaceItem)
    Component.onCompleted: if (playbackManager) playbackManager.attachSurface(surfaceItem)

    function wakeControls() {
        transport.wake()
    }

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

    // Bloom/collapse (DESIGN.md §2.8, §8.1): cross-fade + geometry tween.
    // Reduced-motion → instant swap (no animation).
    opacity: 0
    Behavior on opacity {
        enabled: Theme.motionEnabled
        NumberAnimation { duration: Theme.motionBloom; easing.type: Easing.OutCubic }
    }
    scale: 0.92
    Behavior on scale {
        enabled: Theme.motionEnabled
        NumberAnimation { duration: Theme.motionBloom; easing.type: Easing.OutCubic }
    }

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

    // Cinema top info bar (DESIGN.md §9.2, §9.4): filename · path · n / N with a
    // ✕ that exits Cinema and stops playback. Fades with the bottom transport.
    Rectangle {
        id: cinemaBar
        objectName: "cinemaBar"
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 40
        visible: stage.cinema
        color: Qt.rgba(5/255, 7/255, 13/255, 0.6)
        opacity: stage.cinema ? transport.opacity : 0

        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: Theme.space3
            anchors.rightMargin: Theme.space1
            spacing: Theme.space2

            Text {
                objectName: "cinemaTitle"
                Layout.fillWidth: true
                elide: Text.ElideMiddle
                text: stage.currentName
                      + (stage.currentPath.length > 0 ? "   \u00B7   " + stage.currentPath : "")
                      + (stage.totalCount > 0
                         ? "   \u00B7   " + stage.currentNumber + " / " + stage.totalCount : "")
                color: Theme.textHi
                font.pixelSize: Theme.fontMeta
            }

            ToolButton {
                objectName: "cinemaClose"
                contentItem: Item {
                    Icon { anchors.centerIn: parent; name: "close"; size: 16; color: Theme.textMid }
                }
                onClicked: stage.closeRequested()
                Accessible.name: "Exit fullscreen and stop"
            }
        }
    }

    // Video area: 16:9 max, letterboxed in --ink (DESIGN.md §9.1).
    Item {
        id: stageVideoArea
        anchors.centerIn: parent
        readonly property real availWidth: Math.max(0, stage.width - Theme.space4)
        readonly property real availHeight: Math.max(0, stage.height - Theme.space8)
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

        playing: stage.session ? stage.session.state === 2 : false
        position: stage.session ? stage.session.position : 0
        duration: stage.session ? stage.session.duration : 0
        volume: stage.session ? stage.session.volume : 1.0
        muted: stage.session ? stage.session.muted : false
        seekable: stage.session ? stage.session.seekable : false

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
}
