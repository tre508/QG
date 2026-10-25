import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects

import qg

// Manifest row delegate (DESIGN.md §7.3) — dense columnar search result row.
// Columns: Thumb 96×54, Name flex, Duration 72, Size 88, Modified 120, State 56, Actions 32.
// Row height: 64. Matched substring highlighted in signal blue. No zebra striping.
// Hover lifts to surface2.

Item {
    id: row

    objectName: "manifestRow"

    property string fileName: ""
    property string filePath: ""
    property real durationMs: -1
    property real sizeBytes: 0
    property real mtimeNs: 0
    property bool isFolder: false
    property bool favorite: false
    property bool watched: false
    property int widthPx: 0
    property int heightPx: 0
    property var thumbnail: null
    property var matchRanges: []
    property int thumbnailState: 0

    property bool hovered: false
    property bool selected: false
    property bool focused: false

    // High-DPI scaled dimensions
    readonly property int scaledRowHeight: Theme.scale(64)
    readonly property int scaledThumbWidth: Theme.scale(96)
    readonly property int scaledThumbHeight: Theme.scale(54)
    readonly property int scaledColDuration: Theme.scale(72)
    readonly property int scaledColSize: Theme.scale(88)
    readonly property int scaledColModified: Theme.scale(120)
    readonly property int scaledColState: Theme.scale(56)
    readonly property int scaledColActions: Theme.scale(32)

    readonly property bool isHovered: hovered || hoverArea.containsMouse

    signal clicked()
    signal doubleClicked()

    height: scaledRowHeight
    width: parent ? parent.width : Theme.scale(800)

    // Accessibility per DESIGN.md §2.9
    Accessible.name: `Video, ${row.fileName}, ${Format.duration(row.durationMs)}, ${Format.size(row.sizeBytes)}, ${row.watched ? "watched" : ""} ${row.favorite ? "favorite" : ""}`
    Accessible.role: Accessible.Button

    MouseArea {
        id: hoverArea
        anchors.fill: parent
        hoverEnabled: true
        onClicked: row.clicked()
        onDoubleClicked: row.doubleClicked()
    }

    Rectangle {
        anchors.fill: parent
        color: row.isHovered ? Theme.surface2 : "transparent"

        // Selection fill
        Rectangle {
            anchors.fill: parent
            color: Theme.selectionFill
            visible: row.selected
        }
    }

    // 1px hairline separator (bottom edge)
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        height: 1
        color: Theme.hairline
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: Theme.space1
        spacing: Theme.space3

        // Thumb 96×54
        Rectangle {
            id: thumbCell
            Layout.preferredWidth: scaledThumbWidth
            Layout.preferredHeight: scaledThumbHeight
            radius: Theme.radiusThumbnail
            color: Theme.surface3
            clip: true

            Image {
                anchors.fill: parent
                source: row.thumbnail ? row.thumbnail : ""
                fillMode: Image.PreserveAspectCrop
                asynchronous: true
                visible: row.thumbnail !== null
            }
        }

        // Name (flex) with matched-substring highlight
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            // Highlighted filename
            Text {
                id: nameText
                Layout.fillWidth: true
                text: row.fileName
                color: Theme.textHi
                font.pixelSize: Theme.fontBody
                elide: Text.ElideMiddle
                maximumLineCount: 1

                // Highlight matched substrings in signal blue
                // Subtle highlight via text color override (rich text)
                // For simplicity, rendered with standard text; highlight via delegate integration in P45
            }

            // Containing path (monospace, low color)
            Text {
                Layout.fillWidth: true
                text: row.filePath
                color: Theme.textLow
                font.pixelSize: Theme.fontMicro
                font.family: Theme.fontMono
                elide: Text.ElideMiddle
                maximumLineCount: 1
                visible: row.filePath.length > 0
            }
        }

        // Duration 72
        Text {
            Layout.preferredWidth: scaledColDuration
            text: Format.duration(row.durationMs)
            color: Theme.textMid
            font.pixelSize: Theme.fontMeta
            font.family: Theme.fontMono
            horizontalAlignment: Text.AlignRight
        }

        // Size 88
        Text {
            Layout.preferredWidth: scaledColSize
            text: Format.size(row.sizeBytes)
            color: Theme.textLow
            font.pixelSize: Theme.fontMeta
            font.family: Theme.fontMono
            horizontalAlignment: Text.AlignRight
        }

        // Modified 120
        Text {
            Layout.preferredWidth: scaledColModified
            text: Format.date(row.mtimeNs)
            color: Theme.textLow
            font.pixelSize: Theme.fontMeta
            horizontalAlignment: Text.AlignRight
        }

        // State 56 — glyphs for favorite / watched
        Row {
            Layout.preferredWidth: scaledColState
            spacing: 2
            layoutDirection: Qt.RightToLeft

            Icon {
                visible: row.watched
                name: "watched"
                color: Theme.watched
                size: Theme.fontMicro
            }
            Icon {
                visible: row.favorite
                name: "favorite"
                color: Theme.favorite
                size: Theme.fontMicro
            }
        }

        // Actions 32 — overflow affordance
        Item {
            Layout.preferredWidth: scaledColActions
            Layout.preferredHeight: scaledColActions
            Icon {
                anchors.centerIn: parent
                name: "more"
                color: row.isHovered ? Theme.textMid : Theme.textLow
                size: Theme.fontBody
            }
        }
    }
}