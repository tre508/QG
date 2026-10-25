import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects

import qg

// Video card delegate (DESIGN.md §6). Presentation only: every state is driven
// by injected properties/roles; no domain logic is recomputed here.
//
// Fixed badge/action zones (VIS-012) — each zone has exactly one occupant, so
// combinations cannot collide:
//   TL  favorite (always reserved, y = space1)
//   TL  watched  (always reserved directly beneath, y = space1 + 18)
//   TR  hover quick-action chip (play / favorite / overflow; no trash here)
//   BR  duration chip (+ playing dot)
//   C   single state cue: filmstrip (missing/loading) | amber warn + retry
//       (failed) | lock (unreadable) — the hover play glyph uses the centre
//       only when no state cue owns it
//   left playing ribbon · bottom progress bar · inset focus ring (outside the well)
//
// Placeholder semantics bind to the model's ThumbnailState (0..4); nothing is
// inferred from the image or the filename.

Item {
    id: card

    objectName: "videoCard"

    // Accessibility (Prompt 38): screen-reader announcements per DESIGN.md §2.9
    Accessible.name: `Video, ${card.fileName}, ${Format.duration(card.durationMs)}, ${Format.size(card.sizeBytes)}, ${card.watched ? "watched" : ""} ${card.favorite ? "favorite" : ""} ${card.playing ? "playing" : ""}`
    Accessible.role: Accessible.Button

    property string fileName: ""
    property string path: ""  // Full file path for drag/drop (REM-014 / D-27)
    property real durationMs: -1
    property real sizeBytes: 0
    property real mtimeNs: 0
    property int widthPx: 0
    property int heightPx: 0

    property bool favorite: false
    property bool watched: false
    property real progress: -1 // 0..1; -1 means "no resume cue"
    property int thumbnailState: 0 // 0 None,1 Loading,2 Ready,3 Failed,4 Unreadable
    property var thumbnail: null

    property bool focused: false
    property bool selected: false
    property bool playing: false
    property bool hovered: false
    // Gallery hides the size/date/resolution line (name + duration only).
    property bool detailedMeta: true

    // Inline rename (P62, DESIGN.md §11.3)
    property bool renaming: false
    property bool extensionLock: true
    property var collisionCheck: null // function(name) -> bool (optional)
    signal renameCommitted(string newName)

    readonly property bool isHovered: hovered || hoverArea.containsMouse
    readonly property bool hasProgress: watched || progress > 0
    readonly property real effectiveProgress: watched ? 1.0 : Math.max(0, Math.min(1, progress))
    readonly property bool thumbFailed: thumbnailState === 3
    readonly property bool thumbUnreadable: thumbnailState === 4
    readonly property bool thumbReady: thumbnailState === 2 && thumbnail !== null
    readonly property real wellHeight: Math.round(width * 9 / 16)

    // Centre-zone occupancy (VIS-011/012). The play affordance takes the centre
    // on hover, so the state cues must yield to it; on failure/denied the state
    // cue keeps the centre and the play affordance stands down.
    readonly property bool playGlyphVisible: isHovered && !playing
                                             && !thumbFailed && !thumbUnreadable

    // VIS-011: the well's single state cue, derived from the model state value.
    //   none/loading -> faint filmstrip · failed -> amber warn + retry
    //   unreadable   -> lock cue           · ready   -> the frame itself
    readonly property string placeholderKind: thumbReady ? "ready"
                                             : thumbFailed ? "failed"
                                             : thumbUnreadable ? "unreadable"
                                             : thumbnailState === 1 ? "loading"
                                                                    : "missing"

    // Compact 22px control for the TR hover chip: keeps the zone disjoint from
    // the centred play glyph at the minimum card width (well height 95 -> the
    // glyph starts at y 33 while the chip ends at y 26).
    component ZoneButton: ToolButton {
        id: zoneButton
        implicitWidth: 22
        implicitHeight: 22
        padding: 0
        background: Rectangle {
            radius: Theme.radiusChip
            color: zoneButton.hovered ? Theme.surface3 : "transparent"
        }
    }

    signal clicked()
    signal doubleClicked()
    signal playRequested()
    signal favoriteRequested()
    signal overflowRequested()
    signal thumbnailRetryRequested()

    MouseArea {
        id: hoverArea
        anchors.fill: parent
        hoverEnabled: true
        acceptedButtons: Qt.LeftButton
        drag.target: parent
        drag.axis: Drag.XAndYAxis
        drag.threshold: 10
        drag.minimumX: -parent.width
        drag.minimumY: -parent.height
        drag.maximumX: parent.width * 2
        drag.maximumY: parent.height * 2
        onClicked: card.clicked()
        onDoubleClicked: card.doubleClicked()
    }

    // Drag source: real file URL for in-app and OS-level drag/drop (REM-014 / D-27).
    Drag.active: hoverArea.drag.active
    Drag.hotSpot.x: width / 2
    Drag.hotSpot.y: height / 2
    Drag.mimeData: {
        "text/uri-list": card.path.length > 0 ? ["file://" + card.path] : []
    }
    Drag.dragType: Drag.Automatic

    // --- Card body -----------------------------------------------------------
    Rectangle {
        id: body
        anchors.fill: parent
        radius: Theme.radiusCard
        color: card.isHovered ? Theme.surface3 : Theme.surface2
        border.width: 1
        border.color: card.isHovered ? Theme.hairlineStrong : Theme.hairline

        // Selection (signal blue), 2px border + fill.
        Rectangle {
            objectName: "selectionBorder"
            anchors.fill: parent
            radius: Theme.radiusCard
            color: Theme.selectionFill
            border.width: 2
            border.color: Theme.signal
            visible: card.selected
        }

        // Keyboard focus (cyan), inset — does not change geometry.
        Rectangle {
            objectName: "focusRing"
            anchors.fill: parent
            anchors.margins: 1
            radius: Theme.radiusCard
            color: "transparent"
            border.width: 2
            border.color: Theme.highContrast ? Theme.neonOutline : Theme.focus
            visible: card.focused
        }

        // Playing: mint ribbon + mint border.
        Rectangle {
            objectName: "playingRibbon"
            visible: card.playing
            width: Theme.scale(3)
            radius: 1.5
            color: Theme.highContrast ? Theme.neonOutline : Theme.play
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.margins: 1
        }

        // Thumbnail well -----------------------------------------------------
        Rectangle {
            id: well
            objectName: "thumbnailWell"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.margins: 4
            height: card.wellHeight
            radius: Theme.radiusThumbnail
            color: Theme.surface3
            clip: true

            Image {
                objectName: "thumbImage"
                anchors.fill: parent
                source: card.thumbReady ? card.thumbnail : ""
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                visible: card.thumbReady
                opacity: card.watched ? 0.75 : 1.0
            }

            // Watched desaturation cue (shape/value, colorblind-safe).
            Rectangle {
                anchors.fill: parent
                color: Theme.surface0
                opacity: card.watched ? 0.25 : 0.0
                visible: opacity > 0
            }

            // Centre zone — cue 1 of 3: missing / loading (faint filmstrip).
            Icon {
                objectName: "thumbFilmstrip"
                anchors.centerIn: parent
                visible: !card.thumbReady && !card.thumbFailed && !card.thumbUnreadable
                         && !card.playGlyphVisible
                name: "placeholder"
                color: Theme.textLow
                // Loading is already hinted by the same glyph per the design; the
                // faintness is what keeps it from competing with real imagery.
                opacity: card.placeholderKind === "loading" ? 0.7 : 0.55
                size: Math.round(well.height * 0.4)
            }

            // Centre zone — cue 2 of 3: extraction failure (amber warn + retry).
            // A faint amber scrim makes the well read as a failure zone rather
            // than an icon floating on a blank surface.
            Rectangle {
                objectName: "thumbFailedScrim"
                anchors.fill: parent
                color: Theme.warn
                opacity: card.thumbFailed ? 0.1 : 0.0
                visible: opacity > 0
            }
            Column {
                objectName: "thumbFailedGlyph"
                anchors.centerIn: parent
                visible: card.thumbFailed
                spacing: Theme.space1

                Icon {
                    objectName: "thumbFailedIcon"
                    anchors.horizontalCenter: parent.horizontalCenter
                    name: "warn"
                    color: Theme.warn
                    size: Math.round(well.height * 0.3)
                }
                Button {
                    id: retryButton
                    objectName: "thumbRetry"
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: qsTr("Retry")
                    font.pixelSize: Theme.fontMicro
                    implicitHeight: 20
                    leftPadding: Theme.space2
                    rightPadding: Theme.space2
                    topPadding: 0
                    bottomPadding: 0
                    Accessible.name: qsTr("Retry thumbnail for %1").arg(card.fileName)
                    background: Rectangle {
                        radius: Theme.radiusChip
                        color: retryButton.hovered ? Theme.surface3 : "transparent"
                        border.width: 1
                        border.color: Theme.warn
                    }
                    onClicked: card.thumbnailRetryRequested()
                }
            }

            // Centre zone — cue 3 of 3: access denied (dim + lock).
            Rectangle {
                objectName: "thumbUnreadableScrim"
                anchors.fill: parent
                color: Theme.ink
                opacity: card.thumbUnreadable ? 0.55 : 0.0
                visible: opacity > 0
            }
            Icon {
                objectName: "thumbLockBadge"
                visible: card.thumbUnreadable
                name: "lock"
                color: Theme.warn
                anchors.centerIn: parent
                size: Theme.scale(18)
            }

            // --- Fixed badges -------------------------------------------------
            Icon {
                objectName: "favoriteBadge"
                visible: card.favorite
                name: "favorite"
                color: Theme.favorite
                size: Theme.scale(14)
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.margins: Theme.space1
            }

            // Fixed slot beneath the favourite slot: the watched marker must not
            // move when the favourite toggles (VIS-012 fixed badge zones).
            Icon {
                objectName: "watchedBadge"
                visible: card.watched
                name: "watched"
                color: Theme.watched
                size: Theme.scale(14)
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.topMargin: Theme.space1 + Theme.scale(18)
                anchors.leftMargin: Theme.space1
            }

            // Duration chip (bottom-right) + playing pulse dot.
            Rectangle {
                objectName: "durationChip"
                visible: true // videos always show a duration or "--:--"
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: Theme.space1
                radius: Theme.radiusChip
                color: Theme.chipScrim
                implicitWidth: chipRow.implicitWidth + Theme.space2
                implicitHeight: 16

                Row {
                    id: chipRow
                    anchors.centerIn: parent
                    spacing: Theme.space1

                    Rectangle {
                        objectName: "playingDot"
                        visible: card.playing
                        width: 6
                        height: 6
                        radius: 3
                        color: Theme.play
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Text {
                        objectName: "durationText"
                        text: Format.duration(card.durationMs)
                        color: Theme.textHi
                        font.pixelSize: Theme.fontMicro
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }

            // Hover play glyph (centre, 60%). Yields the centre zone to the
            // failure/denied cues so a play triangle never lands on the amber
            // warn glyph, the lock, or the retry control (VIS-011).
            Icon {
                objectName: "hoverPlayGlyph"
                visible: card.playGlyphVisible
                name: "play"
                color: Theme.textHi
                opacity: 0.6
                size: Theme.scale(28)
                anchors.centerIn: parent
            }

            // Hover quick-action chip (TR) — one fixed zone, one chip background,
            // so the controls read as a group instead of scattered glyphs.
            // Deliberately no trash action here (destructive ops live in the
            // context menu). Hidden while a state cue owns the centre; play /
            // favourite / overflow remain reachable from the overflow menu.
            Rectangle {
                objectName: "quickActions"
                visible: card.isHovered && !card.thumbFailed && !card.thumbUnreadable
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.margins: Theme.space1
                implicitWidth: quickRow.implicitWidth + Theme.space1 * 2
                implicitHeight: 22
                radius: Theme.radiusChip
                color: Theme.chipScrim

                Row {
                    id: quickRow
                    anchors.centerIn: parent
                    spacing: 0

                    ZoneButton {
                        objectName: "quickActionPlay"
                        Accessible.name: qsTr("Play %1").arg(card.fileName)
                        contentItem: Item {
                            implicitWidth: 22
                            implicitHeight: 22
                            Icon {
                                anchors.centerIn: parent
                                name: "play"
                                color: Theme.textHi
                                size: Theme.scale(14)
                            }
                        }
                        onClicked: card.playRequested()
                    }
                    ZoneButton {
                        objectName: "quickActionFavorite"
                        Accessible.name: card.favorite
                            ? qsTr("Remove %1 from favorites").arg(card.fileName)
                            : qsTr("Add %1 to favorites").arg(card.fileName)
                        contentItem: Item {
                            implicitWidth: 22
                            implicitHeight: 22
                            Icon {
                                anchors.centerIn: parent
                                name: "favorite"
                                color: Theme.favorite
                                size: Theme.scale(14)
                            }
                        }
                        onClicked: card.favoriteRequested()
                    }
                    ZoneButton {
                        objectName: "quickActionOverflow"
                        Accessible.name: qsTr("More actions for %1").arg(card.fileName)
                        contentItem: Item {
                            implicitWidth: 22
                            implicitHeight: 22
                            Icon {
                                anchors.centerIn: parent
                                name: "more"
                                color: Theme.textHi
                                size: Theme.scale(14)
                            }
                        }
                        onClicked: card.overflowRequested()
                    }
                }
            }

            // Progress bar (bottom, gradient signal→play).
            Rectangle {
                objectName: "progressBar"
                visible: card.hasProgress
                height: Theme.scale(3)
                width: parent.width * card.effectiveProgress
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: Theme.signal }
                    GradientStop { position: 1.0; color: Theme.play }
                }
            }
        }

        // --- Label + meta -----------------------------------------------------
        Text {
            id: filename
            objectName: "filename"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: well.bottom
            anchors.leftMargin: Theme.space2
            anchors.rightMargin: Theme.space2
            anchors.topMargin: Theme.space1
            text: card.fileName
            elide: Text.ElideMiddle
            color: card.selected ? Theme.textHi
                                 : (card.watched ? Theme.textMid : Theme.textHi)
            font.pixelSize: Theme.fontBody
            visible: !card.renaming
        }

        // Inline rename editor (P62, DESIGN.md §11.3): extension lock + validation.
        TextField {
            id: renameField
            objectName: "renameEditor"
            visible: card.renaming
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: well.bottom
            anchors.leftMargin: Theme.space2
            anchors.rightMargin: Theme.space2
            anchors.topMargin: Theme.space1
            height: 22
            font.pixelSize: Theme.fontBody
            color: Theme.textHi
            selectByMouse: true

            // Border/focus treatment via the background item (P62 validation cue).
            background: Rectangle {
                color: Theme.surface0
                border.color: renameField.renameError.length > 0 ? Theme.danger : Theme.focus
                border.width: 1
                radius: Theme.radiusButton
            }

            property string renameError: ""
            property bool extensionLocked: true

            Component.onCompleted: {
                const dot = card.fileName.lastIndexOf(".")
                if (card.extensionLock && dot > 0) {
                    text = card.fileName
                    cursorPosition = dot
                    select(cursorPosition, text.length) // select the locked extension
                } else {
                    text = card.fileName
                    selectAll()
                }
            }

            onAccepted: {
                if (renameError.length > 0) return
                card.renameCommitted(text)
                card.renaming = false
            }

            Keys.onEscapePressed: {
                card.renaming = false
            }

            // Live validation: red underline when invalid/collision
            onTextChanged: {
                if (text.trim().length === 0 || text === card.fileName) {
                    renameError = ""
                    return
                }
                const dot = text.lastIndexOf(".")
                const base = dot > 0 ? text.left(dot) : text
                if (base.length === 0) {
                    renameError = qsTr("Name cannot be empty")
                    return
                }
                // Collision detection against injected callback (optional)
                if (card.collisionCheck && card.collisionCheck(text))
                    renameError = qsTr("A file named \"%1\" already exists").arg(text)
                else
                    renameError = ""
            }
        }

        Text {
            objectName: "metaLine"
            visible: card.detailedMeta
            anchors.left: parent.left
            anchors.right: parent.right
            // Anchored to the filename, not a fixed offset from the well: the
            // hard-coded 20px collided with the name once font scale rose.
            anchors.top: filename.bottom
            anchors.topMargin: Theme.scale(2)
            anchors.leftMargin: Theme.space2
            anchors.rightMargin: Theme.space2
            elide: Text.ElideRight
            color: Theme.textMid
            font.pixelSize: Theme.fontMeta
            text: {
                const parts = [Format.size(card.sizeBytes), Format.date(card.mtimeNs)]
                if (card.isHovered) {
                    const res = Format.resolution(card.widthPx, card.heightPx)
                    if (res)
                        parts.push(res)
                }
                return parts.join(" \u00B7 ")
            }
        }
    }
}
