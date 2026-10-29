import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects

import qg

// Status strip (DESIGN.md §3.6, §11.4, §12): selection count + aggregate size,
// free space of the current volume, a quiet background-activity cluster with a
// popover, and a transient status slot (toasts land in P64). Reads injected
// state only — no blocking work.

Rectangle {
    id: strip

    objectName: "statusStrip"

    property int selectedCount: 0
    property real selectedBytes: 0
    property real volumeFreeBytes: -1 // < 0 means "unknown" (never fabricate)
    property string volumeLabel: ""
    property int busyCount: 0
    property var tasks: [] // [{ label, count }]
    property string transientText: ""
    // Playback position while a session is live ("Playing 3 / 4", §3.2.6). Empty
    // outside playback, so the strip is unchanged in Browse.
    property string playbackText: ""

    // High-DPI scaled dimensions
    readonly property int scaledStripHeight: Theme.scale(28)
    readonly property int scaledActivitySize: Theme.scale(4)

    // Exposed for tests (a Popup is not part of the visual children tree).
    readonly property alias activityPopupRef: activityPopup

    implicitHeight: scaledStripHeight
    color: Theme.surface1

    function selectionText() {
        if (selectedCount <= 0)
            return qsTr("No selection")
        const label = selectedCount === 1 ? qsTr("1 selected")
                                          : selectedCount + qsTr(" selected")
        return label + " \u00B7 " + Format.size(selectedBytes)
    }

    function freeSpaceText() {
        if (volumeFreeBytes < 0)
            return ""
        const base = qsTr("Free %1").arg(Format.size(volumeFreeBytes))
        return volumeLabel.length > 0 ? (base + " " + qsTr("on") + " " + volumeLabel) : base
    }

    function openActivity() {
        activityPopup.open()
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Theme.space3
        anchors.rightMargin: Theme.space3
        spacing: Theme.space3

        Text {
            objectName: "statusSelection"
            text: strip.selectionText()
            color: Theme.textMid
            font.pixelSize: Theme.fontMeta
        }

        Text {
            objectName: "statusTransient"
            visible: strip.transientText.length > 0
            text: strip.transientText
            color: Theme.textMid
            font.pixelSize: Theme.fontMeta
        }

        // Playback position (§3.2.6): the Reel and the Stage show it too, but the
        // status strip is the always-visible browser-region indicator.
        Text {
            objectName: "statusPlayback"
            visible: strip.playbackText.length > 0
            text: strip.playbackText
            color: Theme.textMid
            font.pixelSize: Theme.fontMeta
        }

        Item { Layout.fillWidth: true }

        Text {
            objectName: "statusFreeSpace"
            visible: strip.freeSpaceText().length > 0
            text: strip.freeSpaceText()
            color: Theme.textMid
            font.pixelSize: Theme.fontMeta
        }

        Item {
            objectName: "statusActivity"
            visible: strip.busyCount > 0
            Layout.preferredWidth: activityRow.implicitWidth
            Layout.preferredHeight: activityRow.implicitHeight

            RowLayout {
                id: activityRow
                spacing: Theme.space1

                Repeater {
                    model: 3
                    delegate: Icon {
                        Layout.alignment: Qt.AlignVCenter
                        name: "activity"
                        color: Theme.play
                        opacity: 0.4
                        size: scaledActivitySize
                        SequentialAnimation on opacity {
                            running: strip.visible
                            loops: Animation.Infinite
                            NumberAnimation { to: 1.0; duration: 400 }
                            NumberAnimation { to: 0.4; duration: 400 }
                        }
                    }
                }

                Text {
                    objectName: "activityCount"
                    Layout.alignment: Qt.AlignVCenter
                    text: strip.busyCount
                    color: Theme.textMid
                    font.pixelSize: Theme.fontMeta
                }
            }

            MouseArea {
                objectName: "activityClickArea"
                anchors.fill: parent
                onClicked: strip.openActivity()
            }
        }
    }

    Popup {
        id: activityPopup
        objectName: "activityPopover"
        x: strip.width - width - Theme.space3
        y: -height - Theme.space1
        padding: Theme.space3

        contentItem: Column {
            spacing: Theme.space1
            Repeater {
                model: strip.tasks
                delegate: Text {
                    text: modelData.label + " \u00B7 " + modelData.count
                    color: Theme.textMid
                    font.pixelSize: Theme.fontMeta
                }
            }
            Text {
                visible: strip.tasks.length === 0
                text: qsTr("No background tasks")
                color: Theme.textLow
                font.pixelSize: Theme.fontMeta
            }
        }
    }
}