import QtQuick
import QtQuick.Controls
import QtQuick.Effects

import qg

// Folder card delegate (DESIGN.md §5.2). Same footprint as a video card, no
// duration chip. Up to four child thumbnails in a 2×2 mosaic, or a calm folder
// glyph when none exist (never a broken image). Presentation only.

Item {
    id: card

    objectName: "folderCard"

    // Accessibility (Prompt 38): screen-reader announcements per DESIGN.md §2.9
    Accessible.name: `Folder, ${card.folderName}, ${card.videoCount} videos, ${card.folderCount} subfolders`
    Accessible.role: Accessible.Button

    property string folderName: ""
    property string path: ""  // Full folder path for drag/drop (REM-014 / D-27)
    property int videoCount: 0
    property int folderCount: 0
    property var mosaicSources: [] // up to four child thumbnail sources

    property bool focused: false
    property bool selected: false
    property bool hovered: false

    readonly property bool isHovered: hovered || hoverArea.containsMouse
    readonly property bool hasMosaic: mosaicSources !== null && mosaicSources.length > 0
    readonly property real wellHeight: Math.round(width * 9 / 16)

    signal clicked()
    signal doubleClicked()

    function countChipText() {
        if (folderCount > 0)
            return videoCount + "v \u00B7 " + folderCount
        return videoCount + "v"
    }

    function metaText() {
        const videos = videoCount === 1 ? qsTr("1 video") : videoCount + qsTr(" videos")
        const folders = folderCount === 1 ? qsTr("1 subfolder") : folderCount + qsTr(" subfolders")
        return videos + " \u00B7 " + folders
    }

    MouseArea {
        id: hoverArea
        anchors.fill: parent
        hoverEnabled: true
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

    // Drag source for folder drag/drop (REM-014 / D-27).
    Drag.active: hoverArea.drag.active
    Drag.hotSpot.x: width / 2
    Drag.hotSpot.y: height / 2
    Drag.mimeData: {
        "text/uri-list": card.path.length > 0 ? ["file://" + card.path] : []
    }
    Drag.dragType: Drag.Automatic

    Rectangle {
        id: body
        anchors.fill: parent
        radius: Theme.radiusCard
        color: card.isHovered ? Theme.surface3 : Theme.surface2
        border.width: 1
        border.color: card.isHovered ? Theme.hairlineStrong : Theme.hairline

        Rectangle {
            objectName: "selectionBorder"
            anchors.fill: parent
            radius: Theme.radiusCard
            color: Theme.selectionFill
            border.width: 2
            border.color: Theme.signal
            visible: card.selected
        }
        Rectangle {
            objectName: "focusRing"
            anchors.fill: parent
            anchors.margins: 1
            radius: Theme.radiusCard
            color: "transparent"
            border.width: 2
            border.color: Theme.focus
            visible: card.focused
        }

        // Thumbnail well: 2×2 mosaic or calm glyph.
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

            Grid {
                anchors.fill: parent
                columns: 2
                spacing: 1
                visible: card.hasMosaic

                Repeater {
                    model: 4
                    delegate: Rectangle {
                        objectName: "mosaicCell"
                        width: (well.width - 1) / 2
                        height: (well.height - 1) / 2
                        color: Theme.surface3

                        Image {
                            anchors.fill: parent
                            source: (card.mosaicSources && index < card.mosaicSources.length)
                                    ? card.mosaicSources[index] : ""
                            fillMode: Image.PreserveAspectCrop
                            asynchronous: true
                        }
                    }
                }
            }

            // Calm folder glyph when there are no child thumbnails.
            Icon {
                objectName: "folderGlyph"
                anchors.centerIn: parent
                visible: !card.hasMosaic
                name: "folder"
                color: Theme.textHi
                opacity: 0.5
                size: Math.round(well.height * 0.35)
            }

            // Count chip (top-right of the well).
            Rectangle {
                objectName: "countChip"
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.margins: Theme.space1
                radius: Theme.radiusChip
                color: Qt.rgba(5 / 255, 7 / 255, 13 / 255, 0.72)
                implicitWidth: countText.implicitWidth + Theme.space2
                implicitHeight: 16

                Text {
                    id: countText
                    objectName: "countChipText"
                    anchors.centerIn: parent
                    text: card.countChipText()
                    color: Theme.textHi
                    font.pixelSize: Theme.fontMicro
                }
            }

            // Hover "enter" affordance (▸), an affordance not a button.
            Icon {
                objectName: "hoverChevron"
                visible: card.isHovered
                name: "chevron-right"
                color: Theme.textHi
                opacity: 0.7
                size: 22
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                anchors.margins: Theme.space1
            }
        }

        // Label (Body-strong) + meta.
        Text {
            objectName: "folderLabel"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: well.bottom
            anchors.leftMargin: Theme.space2
            anchors.rightMargin: Theme.space2
            anchors.topMargin: Theme.space1
            text: card.folderName
            elide: Text.ElideMiddle
            color: card.selected ? Theme.textHi : Theme.textHi
            font.pixelSize: Theme.fontBody
            font.bold: true
        }

        Text {
            objectName: "folderMeta"
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: well.bottom
            anchors.topMargin: 20
            anchors.leftMargin: Theme.space2
            anchors.rightMargin: Theme.space2
            text: card.metaText()
            elide: Text.ElideRight
            color: Theme.textMid
            font.pixelSize: Theme.fontMeta
        }
    }

    // Drop target for folder (REM-014 / D-27): file drop onto folder card.
    DropArea {
        anchors.fill: parent
        keys: ["text/uri-list"]
        onDropped: {
            if (drop.hasUrls && drop.urls.length > 0) {
                const urlStr = drop.urls[0].toString()
                const urlPath = urlStr.indexOf("file://") === 0 ? urlStr.substring(7) : urlStr
                // If it's a folder path being pinned (existing behavior)
                if (drop.mimeData && drop.mimeData["qg/folder-pin"]) {
                    sidebar.pinRequested(urlPath)
                    return
                }
                // File drop onto sidebar place: navigate to folder and handle drop
                if (sidebar.model && sidebar.model.count !== undefined) {
                    for (let i = 0; i < sidebar.model.count; ++i) {
                        const item = sidebar.model.get ? sidebar.model.get(i) : null
                        if (!item) continue
                        const p = (item.path !== undefined) ? item.path : ""
                        if (urlPath === p || urlPath.startsWith(p + "/")) {
                            sidebar.locationActivated(p)
                            sidebar.fileDropped(urlPath, p, !(drop.modifiers & Qt.ControlModifier))
                            return
                        }
                    }
                }
                // If no matching sidebar place, treat as pin attempt
                sidebar.pinRequested(urlPath)
            }
        }
    }
    signal dropReceived(var paths, string targetDir, bool isMove)
}