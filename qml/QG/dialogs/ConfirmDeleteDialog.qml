import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

import qg

// ConfirmDeleteDialog (DESIGN.md §11.1): explicit confirmation for
// permanent deletion. Names the item count. Requires explicit confirm.

ApplicationWindow {
    id: confirmDelete
    objectName: "confirmDeleteDialog"
    title: qsTr("Delete permanently")
    width: 400
    height: 220
    modality: Qt.ApplicationModal
    color: Theme.surface1

    property int itemCount: 1
    property string firstItemName: ""

    signal confirmed()
    signal cancelled()

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Theme.space4
        spacing: Theme.space3

        Text {
            text: {
                if (confirmDelete.itemCount === 1)
                    qsTr("Permanently delete \"%1\"? This cannot be undone.").arg(confirmDelete.firstItemName)
                else
                    qsTr("Permanently delete %1 items? This cannot be undone.").arg(confirmDelete.itemCount)
            }
            color: Theme.danger
            font.pixelSize: Theme.fontBody
            wrapMode: Text.Wrap
        }

        RowLayout {
            spacing: Theme.space2

            Button {
                text: qsTr("Delete permanently")
                onClicked: confirmDelete.confirmed()
            }
            Button {
                text: qsTr("Cancel")
                onClicked: confirmDelete.cancelled()
            }
        }
    }
}