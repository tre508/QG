import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import qg

// PropertiesDialog (DESIGN.md §11.1): non-modal stat/ownership/perms/MIME
// + QG metadata. Native/portal where appropriate; QML surfaces use theme tokens.

ApplicationWindow {
    id: props
    objectName: "propertiesDialog"
    title: qsTr("Properties")
    width: 420
    height: 320
    modality: Qt.NonModal
    color: Theme.surface1

    property string filePath: ""
    property string fileName: ""
    property string size: ""
    property string modified: ""
    property string permissions: ""
    property string owner: ""
    property string mimeType: ""
    property string favorite: ""
    property string watched: ""

    signal closeRequested()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space4
        spacing: Theme.space3

        Text {
            text: props.fileName
            color: Theme.textHi
            font.bold: true
            font.pixelSize: Theme.fontTitle
            elide: Text.ElideMiddle
        }

        GridLayout {
            columns: 2
            Layout.fillWidth: true

            Text { text: qsTr("Path"); color: Theme.textLow; font.pixelSize: Theme.fontMeta }
            Text { text: props.filePath; color: Theme.textMid; font.pixelSize: Theme.fontMeta; Layout.fillWidth: true; elide: Text.ElideMiddle }

            Text { text: qsTr("Size"); color: Theme.textLow; font.pixelSize: Theme.fontMeta }
            Text { text: props.size; color: Theme.textMid; font.pixelSize: Theme.fontMeta }

            Text { text: qsTr("Modified"); color: Theme.textLow; font.pixelSize: Theme.fontMeta }
            Text { text: props.modified; color: Theme.textMid; font.pixelSize: Theme.fontMeta }

            Text { text: qsTr("Permissions"); color: Theme.textLow; font.pixelSize: Theme.fontMeta }
            Text { text: props.permissions; color: Theme.textMid; font.pixelSize: Theme.fontMeta }

            Text { text: qsTr("Owner"); color: Theme.textLow; font.pixelSize: Theme.fontMeta }
            Text { text: props.owner; color: Theme.textMid; font.pixelSize: Theme.fontMeta }

            Text { text: qsTr("Type"); color: Theme.textLow; font.pixelSize: Theme.fontMeta }
            Text { text: props.mimeType; color: Theme.textMid; font.pixelSize: Theme.fontMeta }

            Text { text: qsTr("Favorite"); color: Theme.textLow; font.pixelSize: Theme.fontMeta }
            Text { text: props.favorite; color: Theme.textMid; font.pixelSize: Theme.fontMeta }

            Text { text: qsTr("Watched"); color: Theme.textLow; font.pixelSize: Theme.fontMeta }
            Text { text: props.watched; color: Theme.textMid; font.pixelSize: Theme.fontMeta }
        }

        Button {
            text: qsTr("Close")
            onClicked: props.closeRequested()
        }
    }
}