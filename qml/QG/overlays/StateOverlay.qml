import QtQuick
import QtQuick.Controls

import qg

// StateOverlay (DESIGN.md §12): shared empty/loading/error presentation.
// Shape: glyph + one-line truth + one-line explanation + one primary action.
// Loading never blocks; navigation away cancels enumeration.

Rectangle {
    id: stateOverlay
    objectName: "stateOverlay"

    // state: "empty" | "folderOnly" | "accessDenied" | "disconnected" |
    //        "loading" | "partial" | "error" | "searchNoResults"
    property string state: "empty"
    property string glyph: "\uD83D\uDCC1"
    property string title: ""
    property string explanation: ""
    property string actionLabel: ""
    property string query: ""
    // Disconnected-drive state names the volume (DESIGN.md §12: "says which one").
    property string driveName: ""

    signal actionTriggered()
    signal cancelTriggered()

    color: Theme.surface0
    anchors.fill: parent

    Column {
        anchors.centerIn: parent
        spacing: Theme.space4

        Text {
            anchors.horizontalCenter: parent ? parent.horizontalCenter : undefined
            text: stateOverlay.glyph
            font.pixelSize: 48
            opacity: 0.5
        }

        Text {
            anchors.horizontalCenter: parent ? parent.horizontalCenter : undefined
            text: stateOverlay.title
            color: Theme.textHi
            font.pixelSize: Theme.fontTitle
            horizontalAlignment: Text.AlignHCenter
        }

        Text {
            anchors.horizontalCenter: parent ? parent.horizontalCenter : undefined
            text: stateOverlay.explanation
            color: Theme.textMid
            font.pixelSize: Theme.fontMeta
            horizontalAlignment: Text.AlignHCenter
            visible: text.length > 0
        }

        // Primary action (forward action for every error)
        Button {
            anchors.horizontalCenter: parent ? parent.horizontalCenter : undefined
            text: stateOverlay.actionLabel
            visible: stateOverlay.actionLabel.length > 0
            onClicked: stateOverlay.actionTriggered()
        }

        // Cancel (for partial loading)
        Button {
            anchors.horizontalCenter: parent ? parent.horizontalCenter : undefined
            text: qsTr("Cancel")
            visible: stateOverlay.state === "partial"
            onClicked: stateOverlay.cancelTriggered()
        }
    }

    // Copy rules per state (DESIGN.md §12). Wording never lies: an empty folder
    // is "empty", a permission problem says so, a disconnected drive says which
    // one. `folderOnly` is intentionally NOT an empty state in the browser — the
    // grid renders the folders — Main never selects it (kept for completeness).
    function applyState(s, q) {
        state = s
        query = q
        switch (s) {
        case "empty":
            glyph = "\uD83D\uDCC1"
            title = qsTr("This folder is empty")
            explanation = qsTr("No videos or subfolders here.")
            actionLabel = qsTr("Go to Home")
            break
        case "folderOnly":
            glyph = "\uD83D\uDCC1"
            title = qsTr("No videos here")
            explanation = qsTr("There are subfolders you can navigate into.")
            actionLabel = ""
            break
        case "accessDenied":
            glyph = "\uD83D\uDD12"
            title = qsTr("You do not have permission to open this folder")
            explanation = qsTr("Try opening it as the owner, or check its permissions.")
            actionLabel = qsTr("Copy path")
            break
        case "disconnected":
            glyph = "\u26A0"
            title = driveName.length > 0
                    ? qsTr("%1 is not connected").arg(driveName)
                    : qsTr("The drive is not connected")
            explanation = qsTr("Reconnect the drive and QG will resume.")
            actionLabel = qsTr("Retry")
            break
        case "loading":
            glyph = "\u29D7"
            title = ""            // a skeleton grid + indeterminate bar, no headline
            explanation = ""
            actionLabel = ""
            break
        case "partial":
            glyph = "\u29D7"
            title = qsTr("Loading\u2026")
            explanation = qsTr("Some entries are still being scanned.")
            actionLabel = ""
            break
        case "error":
            glyph = "\u26A0"
            title = qsTr("Something went wrong")
            explanation = qsTr("The operation could not be completed.")
            actionLabel = qsTr("Retry")
            break
        case "searchNoResults":
            glyph = "\uD83D\uDD0D"
            title = qsTr("No matches for \"%1\"").arg(q)
            explanation = qsTr("Try a different search term, or clear the search.")
            actionLabel = qsTr("Clear search")
            break
        default:
            break
        }
    }
}
