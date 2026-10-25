import QtQuick
import QtQuick.Controls

import qg

// ToastHost (DESIGN.md §3.6, §11.4): transient toasts with optional Undo.
// Anchored bottom-center of the content surface. Auto-dismiss 4s;
// Trash undo window ~6s. Toasts never block the UI.

Rectangle {
    id: toastHost
    objectName: "toastHost"

    property string message: ""
    property string undoKind: ""   // "trash" enables Undo
    property string undoPath: ""
    property bool hasUndo: undoKind === "trash"

    signal undoTapped()
    signal detailsTapped()

    anchors.bottom: parent.bottom
    anchors.horizontalCenter: parent.horizontalCenter
    anchors.bottomMargin: Theme.space4
    visible: message.length > 0
    width: row.implicitWidth + Theme.space6
    height: 36
    radius: Theme.radiusButton
    color: Theme.surface3
    border.color: Theme.hairlineStrong
    border.width: 1

    // Auto-dismiss: 4s standard, 6s for Trash undo window
    Timer {
        id: dismissTimer
        interval: toastHost.hasUndo ? 6000 : 4000
        running: toastHost.visible
        onTriggered: toastHost.message = ""
    }

    Row {
        id: row
        anchors.centerIn: parent
        spacing: Theme.space3

        Text {
            text: toastHost.message
            color: Theme.textHi
            font.pixelSize: Theme.fontBody
        }

        Button {
            objectName: "toastUndo"
            text: qsTr("Undo")
            visible: toastHost.hasUndo
            onClicked: toastHost.undoTapped()
        }

        ToolButton {
            objectName: "toastDetails"
            text: "\u2026"
            visible: toastHost.hasUndo
            onClicked: toastHost.detailsTapped()
        }
    }

    function show(text, kind, path) {
        message = text
        undoKind = kind
        undoPath = path
        dismissTimer.restart()
    }
}