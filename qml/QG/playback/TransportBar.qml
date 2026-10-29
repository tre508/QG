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

    // Cinema presentation (DESIGN.md §4.3): the bar is the bottom overlay. In
    // Cinema the controls get larger interaction targets than the inline Stage's,
    // the overlays fade after ~2 s of pointer inactivity instead of the inline
    // 2.5 s (§4.4 vs. §23.5), and the ✕ is omitted — Close belongs to the top
    // overlay there, so "return to Stage" and "close playback" stay distinct
    // (§4.5, fullscreen wireframe).
    property bool cinema: false

    // Overlay visibility (§4.4). The idle timer below owns it; the Stage reads it
    // so the pointer hides exactly while the overlays are hidden.
    property bool shown: true

    // Interaction targets (§4.3): Cinema's are larger than the inline Stage's.
    readonly property int controlSize: cinema ? 40 : 26
    readonly property int glyphSize: cinema ? 24 : 18
    readonly property int primaryGlyphSize: cinema ? 28 : 20
    readonly property int volumeSliderWidth: cinema ? 96 : 72
    readonly property int trackHeight: cinema ? 6 : 4
    // §4.4: "approximately two seconds" in Cinema; §23.5 keeps 2.5 s inline.
    readonly property int idleMs: cinema ? 2000 : 2500

    readonly property bool autoHideActive: autoHideEnabled && !pinned

    // Optimistic scrub state: while dragging, the scrubber shows scrubPosition
    // and the seek is coalesced to a single commit on release.
    property bool scrubbing: false
    property double scrubPosition: 0
    // End of the buffered range in seconds (0 = nothing buffered / unsupported).
    property double bufferedEnd: 0
    // Visual value: the optimistic local value while dragging, else the session
    // position.
    readonly property double displayPosition: scrubbing ? scrubPosition : position
    // §3.1: while the duration is still being probed, the timeline must show a
    // stable "unknown" range instead of repeatedly re-scaling to a guess.
    readonly property bool durationKnown: duration > 0
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

    // §4.3: a taller bar in Cinema (the video area already reserves the bar's
    // height, so the video shrinks to make room instead of being covered).
    height: cinema ? 64 : 48
    color: Qt.rgba(5/255, 7/255, 13/255, 0.85)

    // The overlay fades as one unit (§4.4) and stops accepting input the moment
    // it is hidden, so an invisible control can never be clicked.
    opacity: shown ? 1 : 0
    Behavior on opacity {
        enabled: Theme.motionEnabled
        NumberAnimation { duration: Theme.motionStandard }
    }
    enabled: shown

    function timeText(seconds) {
        if (!(seconds > 0) || !isFinite(seconds))
            return "0:00"
        return Format.duration(seconds * 1000)
    }

    // Total duration, or an explicit unknown marker while it is being probed
    // (§3.1) — never a fabricated 0:00 that looks like a real length.
    function durationText() {
        return durationKnown ? Format.duration(duration * 1000) : "--:--"
    }

    // Timeline position → seconds. The handle centre travels between
    // handleWidth/2 and width - handleWidth/2 in the default style.
    function valueAtX(px) {
        const hw = timeline.handle ? timeline.handle.width : 0
        const travel = Math.max(1, timeline.width - hw)
        const frac = Math.max(0, Math.min(1, (px - hw / 2) / travel))
        return timeline.from + frac * (timeline.to - timeline.from)
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

    // Overlay auto-hide timer (§4.4). Deliberately NOT a `running:` binding:
    // wake() has to restart the countdown, and an imperative restart would break
    // a bound `running`, leaving the timer armed where auto-hide is off — which is
    // how the inline Stage (whose controls never hide, §2.1.6) lost its controls
    // 2.5 s after every return from Cinema.
    Timer {
        id: autoHideTimer
        objectName: "transportAutoHideTimer"
        interval: transport.idleMs
        repeat: false
        onTriggered: transport.shown = false
    }

    Component.onCompleted: if (autoHideActive) autoHideTimer.restart()
    onAutoHideActiveChanged: if (autoHideActive) autoHideTimer.restart()
                             else autoHideTimer.stop()

    // Returns the overlays to full opacity and restarts the idle countdown (§4.4).
    function wake() {
        transport.shown = true
        if (autoHideActive)
            autoHideTimer.restart()
        else
            autoHideTimer.stop()
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
        // Wider gutters in Cinema, so the larger targets are not cramped (§4.3).
        anchors.margins: transport.cinema ? Theme.space2 : Theme.space1
        spacing: transport.cinema ? Theme.space2 : Theme.space1

        ToolButton {
            objectName: "transportPrev"
            implicitWidth: transport.controlSize
            implicitHeight: transport.controlSize
            contentItem: Item {
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -4
                    radius: 6
                    color: "transparent"
                    border.color: parent.parent.activeFocus ? (Theme.highContrast ? Theme.neonOutline : Theme.focus) : "transparent"
                    border.width: parent.parent.activeFocus ? 2 : 0
                }
                Icon { anchors.centerIn: parent; name: "prev"; size: transport.glyphSize; color: Theme.textMid }
            }
            onClicked: transport.previousRequested()
            Accessible.name: "Previous"
            Accessible.role: Accessible.Button
        }

        ToolButton {
            objectName: "transportSeekBack"
            implicitWidth: transport.controlSize
            implicitHeight: transport.controlSize
            contentItem: Item {
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -4
                    radius: 6
                    color: "transparent"
                    border.color: parent.parent.activeFocus ? (Theme.highContrast ? Theme.neonOutline : Theme.focus) : "transparent"
                    border.width: parent.parent.activeFocus ? 2 : 0
                }
                Icon { anchors.centerIn: parent; name: "seek-back"; size: transport.glyphSize; color: Theme.textMid }
            }
            onClicked: transport.seekBackRequested()
            Accessible.name: "Rewind 10 seconds"
        }

        ToolButton {
            objectName: "transportPlayPause"
            implicitWidth: transport.controlSize
            implicitHeight: transport.controlSize
            contentItem: Item {
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -4
                    radius: 6
                    color: "transparent"
                    border.color: parent.parent.activeFocus ? (Theme.highContrast ? Theme.neonOutline : Theme.focus) : "transparent"
                    border.width: parent.parent.activeFocus ? 2 : 0
                }
                Icon { objectName: "transportPlayPauseIcon"; anchors.centerIn: parent; name: transport.playing ? "pause" : "play"; size: transport.primaryGlyphSize; color: Theme.textHi }
            }
            onClicked: transport.playPauseRequested()
            Accessible.name: transport.playing ? "Pause" : "Play"
        }

        ToolButton {
            objectName: "transportSeekForward"
            implicitWidth: transport.controlSize
            implicitHeight: transport.controlSize
            contentItem: Item {
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -4
                    radius: 6
                    color: "transparent"
                    border.color: parent.parent.activeFocus ? (Theme.highContrast ? Theme.neonOutline : Theme.focus) : "transparent"
                    border.width: parent.parent.activeFocus ? 2 : 0
                }
                Icon { anchors.centerIn: parent; name: "seek-forward"; size: transport.glyphSize; color: Theme.textMid }
            }
            onClicked: transport.seekForwardRequested()
            Accessible.name: "Forward 10 seconds"
        }

        ToolButton {
            objectName: "transportNext"
            implicitWidth: transport.controlSize
            implicitHeight: transport.controlSize
            contentItem: Item {
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -4
                    radius: 6
                    color: "transparent"
                    border.color: parent.parent.activeFocus ? (Theme.highContrast ? Theme.neonOutline : Theme.focus) : "transparent"
                    border.width: parent.parent.activeFocus ? 2 : 0
                }
                Icon { anchors.centerIn: parent; name: "next"; size: transport.glyphSize; color: Theme.textMid }
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
            // Stable while the duration is unknown (§3.1): the range never
            // re-scales to a guess and re-scales again on every observation.
            to: transport.durationKnown ? transport.duration : 1

            // Hover timestamp (§3.1: "hover tooltip showing the corresponding
            // timestamp"). The tooltip itself lives beside the RowLayout so it can
            // overflow the row's bounds.
            HoverHandler {
                id: timelineHover
                enabled: !transport.scrubbing
            }

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
                height: transport.trackHeight
                color: Theme.surface4
                radius: 2

                // Buffered range (§3.1, "when supported"): drawn behind the
                // progress indicator; mpv may report nothing cached, in which
                // case the bar is simply absent.
                Rectangle {
                    objectName: "transportBuffered"
                    visible: transport.bufferedEnd > 0
                    width: parent.width * Math.min(1, transport.bufferedEnd
                                                   / (transport.durationKnown ? transport.duration : 1))
                    height: parent.height
                    color: Theme.surface3
                    radius: 2
                }

                Rectangle {
                    width: parent.width * (transport.displayPosition
                                           / (transport.durationKnown ? transport.duration : 1))
                    height: parent.height
                    color: transport.seekRejected ? Theme.warn : Theme.signal
                    radius: 2
                }
            }
        }

        Text {
            objectName: "transportTime"
            text: transport.timeText(transport.displayPosition) + " / "
                  + transport.durationText()
            color: Theme.textMid
            font.pixelSize: Theme.fontMicro
            font.family: Theme.fontMono
        }

        // Volume control: 0..1 fraction, matches the session contract.
        ToolButton {
            objectName: "transportMute"
            implicitWidth: transport.controlSize
            implicitHeight: transport.controlSize
            contentItem: Item {
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -4
                    radius: 6
                    color: "transparent"
                    border.color: parent.parent.activeFocus ? (Theme.highContrast ? Theme.neonOutline : Theme.focus) : "transparent"
                    border.width: parent.parent.activeFocus ? 2 : 0
                }
                Icon { anchors.centerIn: parent; name: transport.muted ? "mute" : "volume"; size: transport.glyphSize; color: Theme.textMid }
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
            implicitWidth: transport.volumeSliderWidth
            Accessible.role: Accessible.Slider
            Accessible.description: "Volume " + Math.round(transport.volume * 100) + " percent"
        }

        ToolButton {
            objectName: "transportFullscreen"
            implicitWidth: transport.controlSize
            implicitHeight: transport.controlSize
            contentItem: Item {
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -4
                    radius: 6
                    color: "transparent"
                    border.color: parent.parent.activeFocus ? (Theme.highContrast ? Theme.neonOutline : Theme.focus) : "transparent"
                    border.width: parent.parent.activeFocus ? 2 : 0
                }
                Icon { anchors.centerIn: parent; name: "fullscreen"; size: transport.glyphSize; color: Theme.textMid }
            }
            onClicked: transport.fullscreenRequested()
            Accessible.name: "Fullscreen"
        }

        ToolButton {
            objectName: "transportClose"
            implicitWidth: transport.controlSize
            implicitHeight: transport.controlSize
            // §4.3/§4.5: Cinema's bottom overlay carries ⛶ (exit fullscreen) and
            // NOT Close — the explicit Close button lives in the top overlay, so
            // "return to Stage" and "close playback" cannot be confused.
            visible: !transport.cinema
            contentItem: Item {
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: -4
                    radius: 6
                    color: "transparent"
                    border.color: parent.parent.activeFocus ? (Theme.highContrast ? Theme.neonOutline : Theme.focus) : "transparent"
                    border.width: parent.parent.activeFocus ? 2 : 0
                }
                Icon { anchors.centerIn: parent; name: "close"; size: transport.glyphSize; color: Theme.textMid }
            }
            onClicked: transport.closeRequested()
            Accessible.name: "Close"
        }
    }

    // Hover timestamp for the timeline (§3.1). Declared outside the RowLayout so
    // it can sit above the row instead of being clipped to it; it never accepts
    // input, so the drag/click gesture stays with the slider.
    Rectangle {
        id: seekTooltip
        objectName: "transportSeekTooltip"
        visible: timelineHover.hovered && transport.durationKnown
        width: tooltipText.implicitWidth + Theme.space2
        height: tooltipText.implicitHeight + Theme.space1
        radius: Theme.radiusChip
        color: Theme.surface3
        border.color: Theme.hairline
        border.width: 1

        // Centre on the pointer, clamped to the bar so it never runs off either
        // edge of the transport.
        x: Math.max(0, Math.min(transport.width - width,
                                  timeline.mapToItem(transport, timelineHover.point.position.x, 0).x
                                  - width / 2))
        y: timeline.mapToItem(transport, 0, 0).y - height - Theme.space1

        Text {
            id: tooltipText
            objectName: "transportSeekTooltipText"
            anchors.centerIn: parent
            text: transport.timeText(transport.valueAtX(timelineHover.point.position.x))
            color: Theme.textHi
            font.pixelSize: Theme.fontMicro
            font.family: Theme.fontMono
        }
    }
}
