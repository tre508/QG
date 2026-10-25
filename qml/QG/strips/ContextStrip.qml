import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects

import qg

// Context strip (DESIGN.md §3.6, §4): current folder name, item counts, active
// order and density selector. Collapses to name+order at 900–1150px and hides
// below 900px. Reads view state only — no domain logic.

Rectangle {
    id: strip

    objectName: "contextStrip"
    property real availableWidth: width
    property string folderName: ""
    property int videoCount: 0
    property int folderCount: 0
    property string orderLabel: ""
    property int density: 1 // ContentView.Density

    // High-DPI scaled dimensions
    readonly property int scaledStripHeight: Theme.scale(28)

    readonly property bool compact: availableWidth < 1150
    visible: availableWidth >= 900
    implicitHeight: scaledStripHeight
    color: Theme.surface0

    signal densitySelected(int density)

    function countsText() {
        const videos = videoCount === 1 ? qsTr("1 video") : videoCount + qsTr(" videos")
        const folders = folderCount === 1 ? qsTr("1 folder") : folderCount + qsTr(" folders")
        return videos + ", " + folders
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.space3
        anchors.rightMargin: Theme.space3
        spacing: Theme.space3

        Text {
            objectName: "contextFolderName"
            text: strip.folderName
            color: Theme.textHi
            font.pixelSize: Theme.fontMeta
            font.bold: true
            elide: Text.ElideRight
            Layout.maximumWidth: strip.width * 0.4
        }

        Text {
            objectName: "contextCounts"
            visible: !strip.compact
            text: strip.countsText()
            color: Theme.textMid
            font.pixelSize: Theme.fontMeta
        }

        Item { Layout.fillWidth: true }

        Text {
            objectName: "contextOrder"
            text: strip.orderLabel
            color: Theme.textMid
            font.pixelSize: Theme.fontMeta
        }

        Row {
            objectName: "densitySelector"
            visible: !strip.compact
            spacing: Theme.space1

            Repeater {
                model: [
                    { icon: "density-gallery", value: 0,
                      label: qsTr("Gallery"), shortcut: "Ctrl+1" },
                    { icon: "density-standard", value: 1,
                      label: qsTr("Standard"), shortcut: "Ctrl+2" },
                    { icon: "density-compact", value: 2,
                      label: qsTr("Compact"), shortcut: "Ctrl+3" }
                ]

                delegate: ToolButton {
                    objectName: "densityOption"
                    // The strip is `scaledStripHeight` (28 px) tall; default
                    // ToolButton padding would make the options 36 px and
                    // overflow it.
                    padding: 0
                    implicitWidth: Theme.scale(24)
                    implicitHeight: Theme.scale(24)
                    // The mode must be identifiable without opening anything:
                    // name it for pointer users and for screen readers.
                    ToolTip.text: modelData.label + " (" + modelData.shortcut + ")"
                    ToolTip.visible: hovered
                    Accessible.name: qsTr("View density: %1").arg(modelData.label)
                    Accessible.checkable: true
                    Accessible.checked: strip.density === modelData.value
                    contentItem: Item {
                        implicitWidth: Theme.scale(24)
                        implicitHeight: Theme.scale(24)
                        // Active state is a filled chip, not just a hue shift.
                        Rectangle {
                            anchors.fill: parent
                            radius: Theme.radiusChip
                            color: strip.density === modelData.value
                                   ? Theme.surface3 : "transparent"
                            border.width: strip.density === modelData.value ? 1 : 0
                            border.color: Theme.hairlineStrong
                        }
                        Icon {
                            anchors.centerIn: parent
                            name: modelData.icon
                            color: strip.density === modelData.value
                                   ? Theme.textHi : Theme.textMid
                            size: Theme.scale(16)
                        }
                    }
                    checkable: true
                    checked: strip.density === modelData.value
                    onClicked: strip.densitySelected(modelData.value)
                }
            }
        }
    }
}