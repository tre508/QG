import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects

import qg

// Compact density row (DESIGN.md §5.4, Appendix A): 96×54 thumbnail with name,
// duration, size, date and state badges in aligned (tabular) columns. Renders
// both video and folder rows. Presentation only; distinct from the search
// Manifest (P43).

Item {
    id: row

    objectName: "compactRow"

    property string fileName: ""
    property string path: ""  // Full file path for drag/drop (REM-014 / D-27)
    property bool isFolder: false
    property real durationMs: -1
    property real sizeBytes: 0
    property real mtimeNs: 0
    property int widthPx: 0
    property int heightPx: 0
    property int videoCount: 0
    property int folderCount: 0

    property bool favorite: false
    property bool watched: false
    property bool playing: false
    property int thumbnailState: 0

    property bool focused: false
    property bool selected: false
    property bool hovered: false

    readonly property bool isHovered: hovered || hoverArea.containsMouse
    readonly property bool thumbFailed: thumbnailState === 3
    readonly property bool thumbUnreadable: thumbnailState === 4

    // High-DPI scaled dimensions
    readonly property int scaledRowHeight: Theme.scale(60)
    readonly property int scaledThumbWidth: Theme.scale(96)
    readonly property int scaledThumbHeight: Theme.scale(54)

    signal clicked()
    signal doubleClicked()

    height: scaledRowHeight

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
        onClicked: row.clicked()
        onDoubleClicked: row.doubleClicked()
    }

    // Drag source for compact row (REM-014 / D-27).
    Drag.active: hoverArea.drag.active
    Drag.hotSpot.x: width / 2
    Drag.hotSpot.y: height / 2
    Drag.mimeData: {
        "text/uri-list": row.path.length > 0 ? ["file://" + row.path] : []
    }
    Drag.dragType: Drag.Automatic

    Rectangle {
        anchors.fill: parent
        color: row.isHovered ? Theme.surface2 : "transparent"
    }

    Rectangle {
        objectName: "selectionBorder"
        anchors.fill: parent
        color: Theme.selectionFill
        border.width: 2
        border.color: Theme.signal
        visible: row.selected
    }

    Rectangle {
        objectName: "focusRing"
        anchors.fill: parent
        anchors.margins: 1
        color: "transparent"
        border.width: 2
        border.color: Theme.focus
        visible: row.focused
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.space3
        anchors.rightMargin: Theme.space3
        spacing: Theme.space3

        // 96×54 thumbnail
        Rectangle {
            objectName: "compactThumb"
            Layout.preferredWidth: scaledThumbWidth
            Layout.preferredHeight: scaledThumbHeight
            radius: Theme.radiusThumbnail
            color: Theme.surface3
            clip: true

            Icon {
                anchors.centerIn: parent
                visible: !row.isFolder && !row.thumbFailed && !row.thumbUnreadable
                name: "placeholder"
                color: Theme.textLow
                opacity: 0.6
                size: Theme.scale(24)
            }
            Icon {
                anchors.centerIn: parent
                visible: row.isFolder
                name: "folder"
                color: Theme.textHi
                opacity: 0.5
                size: Theme.scale(24)
            }
            Icon {
                anchors.centerIn: parent
                visible: row.thumbFailed
                name: "warn"
                color: Theme.warn
                size: Theme.scale(20)
            }
            Icon {
                anchors.centerIn: parent
                visible: row.thumbUnreadable
                name: "lock"
                color: Theme.warn
                size: Theme.scale(18)
            }
            Rectangle {
                objectName: "compactPlayingRibbon"
                visible: row.playing
                width: Theme.scale(3)
                color: Theme.play
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.bottom: parent.bottom
            }
        }

        Text {
            objectName: "compactName"
            Layout.fillWidth: true
            text: row.isFolder
                  ? (row.folderCount > 0 ? row.fileName + " \u00B7 " + row.videoCount + "v"
                                         : row.fileName)
                  : row.fileName
            elide: Text.ElideMiddle
            color: row.watched ? Theme.textMid : Theme.textHi
            font.pixelSize: Theme.fontBody
            font.bold: row.isFolder
        }

        Text {
            objectName: "compactDuration"
            Layout.preferredWidth: 56
            horizontalAlignment: Text.AlignRight
            text: row.isFolder ? "" : Format.duration(row.durationMs)
            color: Theme.textMid
            font.pixelSize: Theme.fontMeta
            font.family: Theme.fontMono
        }
        Text {
            objectName: "compactSize"
            Layout.preferredWidth: 80
            horizontalAlignment: Text.AlignRight
            text: row.isFolder ? "" : Format.size(row.sizeBytes)
            color: Theme.textMid
            font.pixelSize: Theme.fontMeta
            font.family: Theme.fontMono
        }
        Text {
            objectName: "compactDate"
            Layout.preferredWidth: 90
            horizontalAlignment: Text.AlignRight
            text: Format.date(row.mtimeNs)
            color: Theme.textMid
            font.pixelSize: Theme.fontMeta
            font.family: Theme.fontMono
        }

        Row {
            objectName: "compactBadges"
            Layout.preferredWidth: Theme.scale(40)
            spacing: Theme.space1
            Icon {
                objectName: "compactFavorite"
                visible: row.favorite
                name: "favorite"
                color: Theme.favorite
                size: Theme.scale(12)
            }
            Icon {
                objectName: "compactWatched"
                visible: row.watched
                name: "watched"
                color: Theme.watched
                size: Theme.scale(12)
            }
        }
    }
}