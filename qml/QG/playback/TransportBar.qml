import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import qg

// Transport bar (DESIGN.md §8.2, §8.3): play/pause, prev/next, ±10s seek,
// seek timeline, volume/mute, fullscreen, close. Auto-hide 2.5s, Tab pins
// visible. Presentation only: it never touches playback directly — every
// control forwards an intent to the owner (Stage → PlaybackManager/Controller).
//
// Seek gestures are optimistic and coalesced (DESIGN.md §8.3): while the user
// drags the scrubber it moves immediately against a local value and the seek is
// committed once on release; a non-seekable stream snaps back with an amber
// flash instead of moving.

Rectangle {
    id: transport
    objectName: "transportBar"

    property bool playing: false
    property double position: 0
    property double duration: 0
    // Canonical 0..1 fraction (PlaybackSession contract).
    property double volume: 1.0
    property bool muted: false
    property bool seekable: true
    property string currentName: ""

    property bool pinned: false
    property bool autoHideEnabled: true

    // Optimistic scrub state: while dragging, the scrubber shows scrubPosition
    // and the seek is coalesced to a single commit on release.
    property bool scrubbing: false
    property double scrubPosition: 0
    // Visual value: the optimistic local value while dragging, else the session
    // position.
    readonly property double displayPosition: scrubbing ? scrubPosition : position
    // Non-seekable feedback (DESIGN.md §8.3): brief amber flash, scrubber snaps back.
    property bool seekRejected: false

    signal playPauseRequested()
    signal previousRequested()
    signal nextRequested()
    signal seekForwardRequested()
    signal seekBackRequested()
    signal seekToRequested(double position)
    signal volumeRequested(double vol)
    signal muteToggled()
    signal fullscreenRequested()
    signal closeRequested()

    height: 48
    color: Qt.rgba(5/255, 7/255, 13/255, 0.85)

    function timeText(seconds) {
        if (!(seconds > 0) || !isFinite(seconds))
            return "0:00"
        return Format.duration(seconds * 1000)
    }

    // --- Seek gesture contract (optimistic + coalesced, DESIGN.md §8.3) ------
    // beginScrub: the pointer goes down; the scrubber starts tracking a local,
    // optimistic value. scrubTo: pointer moved. commitScrub: pointer released —
    // the single coalesced seek is issued here (or the request is rejected on a
    // non-seekable stream with an amber flash).
    function beginScrub(atValue) {
        scrubbing = true
        scrubPosition = atValue
    }

    function scrubTo(value) {
        if (!seekable) {
            seekRejected = true
            rejectTimer.restart()
            return
        }
        scrubPosition = value
    }

    function commitScrub() {
        scrubbing = false
        if (seekable)
            seekToRequested(scrubPosition)
        timeline.restoreTracking()
    }

    // Called by the Stage when PlaybackManager::seekRejected() fires: the
    // manager already restored the real position, so the block only shows the
    // "not seekable" affordance (DESIGN.md playback §3.1).
    function flashSeekRejected() {
        seekRejected = true
        rejectTimer.restart()
    }

    // Auto-hide timer: 2.5s after the last input (DESIGN.md §8.2). Pinned (Tab
    // focus) keeps the bar visible.
    Timer {
        id: autoHideTimer
        interval: 2500
        running: transport.autoHideEnabled && !transport.pinned
        onTriggered: transport.opacity = 0
    }

    // Returns the bar to full opacity and restarts the idle countdown.
    function wake() {
        transport.opacity = 1
        autoHideTimer.restart()
    }

    // Amber flash when a seek is refused because the stream is not seekable.
    Timer {
        id: rejectTimer
        interval: 400
        onTriggered: transport.seekRejected = false
    }

    MouseArea {
        anchors.fill: parent
        hoverEnabled: true
        onEntered: transport.wake()
        onPositionChanged: transport.wake()
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: Theme.space1
        spacing: Theme.space1

        ToolButton {
            objectName: "transportPrev"
            contentItem: Item {
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -4
                    radius: 6
                    color: "transparent"
                    border.color: parent.parent.activeFocus ? (Theme.highContrast ? Theme.neonOutline : Theme.focus) : "transparent"
                    border.width: parent.parent.activeFocus ? 2 : 0
                }
                Icon { anchors.centerIn: parent; name: "prev"; size: 18; color: Theme.textMid }
            }
            onClicked: transport.previousRequested()
            Accessible.name: "Previous"
            Accessible.role: Accessible.Button
        }

        ToolButton {
            objectName: "transportSeekBack"
            contentItem: Item {
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -4
                    radius: 6
                    color: "transparent"
                    border.color: parent.parent.activeFocus ? (Theme.highContrast ? Theme.neonOutline : Theme.focus) : "transparent"
                    border.width: parent.parent.activeFocus ? 2 : 0
                }
                Icon { anchors.centerIn: parent; name: "seek-back"; size: 18; color: Theme.textMid }
            }
            onClicked: transport.seekBackRequested()
            Accessible.name: "Rewind 10 seconds"
        }

        ToolButton {
            objectName: "transportPlayPause"
            contentItem: Item {
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -4
                    radius: 6
                    color: "transparent"
                    border.color: parent.parent.activeFocus ? (Theme.highContrast ? Theme.neonOutline : Theme.focus) : "transparent"
                    border.width: parent.parent.activeFocus ? 2 : 0
                }
                Icon { anchors.centerIn: parent; name: transport.playing ? "pause" : "play"; size: 20; color: Theme.textHi }
            }
            onClicked: transport.playPauseRequested()
            Accessible.name: transport.playing ? "Pause" : "Play"
        }

        ToolButton {
            objectName: "transportSeekForward"
            contentItem: Item {
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -4
                    radius: 6
                    color: "transparent"
                    border.color: parent.parent.activeFocus ? (Theme.highContrast ? Theme.neonOutline : Theme.focus) : "transparent"
                    border.width: parent.parent.activeFocus ? 2 : 0
                }
                Icon { anchors.centerIn: parent; name: "seek-forward"; size: 18; color: Theme.textMid }
            }
            onClicked: transport.seekForwardRequested()
            Accessible.name: "Forward 10 seconds"
        }

        ToolButton {
            objectName: "transportNext"
            contentItem: Item {
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -4
                    radius: 6
                    color: "transparent"
                    border.color: parent.parent.activeFocus ? (Theme.highContrast ? Theme.neonOutline : Theme.focus) : "transparent"
                    border.width: parent.parent.activeFocus ? 2 : 0
                }
                Icon { anchors.centerIn: parent; name: "next"; size: 18; color: Theme.textMid }
            }
            onClicked: transport.nextRequested()
            Accessible.name: "Next"
        }

        // Seek timeline: optimistic + coalesced (DESIGN.md §8.3). `pressed`
        // drives the optimistic value; the commit happens once on release.
        Slider {
            id: timeline
            objectName: "transportTimeline"
            Layout.fillWidth: true
            from: 0
            to: transport.duration > 0 ? transport.duration : 1

            // Track the session position except while the user is dragging, when
            // the control's own value (the optimistic scrub position) wins.
            // The binding is explicitly restored after each gesture because a
            // user drag breaks it.
            Component.onCompleted: restoreTracking()

            function restoreTracking() {
                timeline.value = Qt.binding(function () {
                    return transport.scrubbing ? transport.scrubPosition
                                               : transport.position
                })
            }

            onMoved: transport.scrubTo(value)
            onPressedChanged: {
                if (pressed)
                    transport.beginScrub(value)
                else
                    transport.commitScrub()
            }

            Accessible.role: Accessible.Slider
            Accessible.description: Math.floor(transport.position) + " of "
                                    + Math.floor(transport.duration) + " seconds"

            background: Rectangle {
                height: 4
                color: Theme.surface4
                radius: 2

                Rectangle {
                    width: parent.width * (transport.displayPosition
                                           / (transport.duration > 0 ? transport.duration : 1))
                    height: parent.height
                    color: transport.seekRejected ? Theme.warn : Theme.signal
                    radius: 2
                }
            }
        }

        Text {
            objectName: "transportTime"
            text: transport.timeText(transport.displayPosition) + " / "
                  + transport.timeText(transport.duration)
            color: Theme.textMid
            font.pixelSize: Theme.fontMicro
            font.family: Theme.fontMono
        }

        // Volume control: 0..1 fraction, matches the session contract.
        ToolButton {
            objectName: "transportMute"
            contentItem: Item {
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -4
                    radius: 6
                    color: "transparent"
                    border.color: parent.parent.activeFocus ? (Theme.highContrast ? Theme.neonOutline : Theme.focus) : "transparent"
                    border.width: parent.parent.activeFocus ? 2 : 0
                }
                Icon { anchors.centerIn: parent; name: transport.muted ? "mute" : "volume"; size: 18; color: Theme.textMid }
            }
            onClicked: transport.muteToggled()
            Accessible.name: transport.muted ? "Unmute" : "Mute"
        }

        Slider {
            objectName: "transportVolume"
            from: 0
            to: 1
            value: transport.volume
            onMoved: transport.volumeRequested(value)
            implicitWidth: 72
            Accessible.role: Accessible.Slider
            Accessible.description: "Volume " + Math.round(transport.volume * 100) + " percent"
        }

        ToolButton {
            objectName: "transportFullscreen"
            contentItem: Item {
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -4
                    radius: 6
                    color: "transparent"
                    border.color: parent.parent.activeFocus ? (Theme.highContrast ? Theme.neonOutline : Theme.focus) : "transparent"
                    border.width: parent.parent.activeFocus ? 2 : 0
                }
                Icon { anchors.centerIn: parent; name: "fullscreen"; size: 18; color: Theme.textMid }
            }
            onClicked: transport.fullscreenRequested()
            Accessible.name: "Fullscreen"
        }

        ToolButton {
            objectName: "transportClose"
            contentItem: Item {
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -4
                    radius: 6
                    color: "transparent"
                    border.color: parent.parent.activeFocus ? (Theme.highContrast ? Theme.neonOutline : Theme.focus) : "transparent"
                    border.width: parent.parent.activeFocus ? 2 : 0
                }
                Icon { anchors.centerIn: parent; name: "close"; size: 18; color: Theme.textMid }
            }
            onClicked: transport.closeRequested()
            Accessible.name: "Close"
        }
    }
}
