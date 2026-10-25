import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import qg

// RenameConflictDialog (DESIGN.md §11.1): shows the collision and offers
// Overwrite / Keep both / Cancel.

ApplicationWindow {
    id: conflict
    objectName: "renameConflictDialog"
    title: qsTr("Rename conflict")
    width: 380
    height: 200
    modality: Qt.ApplicationModal
    color: Theme.surface1

    property string targetName: ""

    signal overwriteConfirmed()
    signal keepBothConfirmed()
    signal cancelConfirmed()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space4
        spacing: Theme.space3

        Text {
            text: qsTr("A file named \"%1\" already exists").arg(conflict.targetName)
            color: Theme.textHi
            font.pixelSize: Theme.fontBody
            wrapMode: Text.Wrap
        }

        RowLayout {
            spacing: Theme.space2

            Button {
                text: qsTr("Overwrite")
                onClicked: conflict.overwriteConfirmed()
            }
            Button {
                text: qsTr("Keep both")
                onClicked: conflict.keepBothConfirmed()
            }
            Button {
                text: qsTr("Cancel")
                onClicked: conflict.cancelConfirmed()
            }
        }
    }
}