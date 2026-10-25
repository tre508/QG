import QtQuick
import QtQuick.Controls

import qg

// No-results overlay (DESIGN.md §7.4, Prompt 44).
// Centered presentation when search returns zero matches.
// Copy: "No matches for \"query\"" — never "No videos found".
// Clear search button + inert "Search subfolders (soon)" affordance.

Rectangle {
    id: root

    property string query: ""
    signal clearRequested()

    color: Theme.ink
    anchors.fill: parent

    Column {
        anchors.centerIn: parent
        spacing: Theme.space4

        // Large search-fallback glyph
        Text {
            anchors.horizontalCenter: parent ? parent.horizontalCenter : undefined
            text: "\uD83D\uDD0D" // magnifying glass emoji
            font.pixelSize: 48
            opacity: 0.5
        }

        Text {
            anchors.horizontalCenter: parent ? parent.horizontalCenter : undefined
            text: root.query.length > 0
                  ? "No matches for \"" + root.query + "\""
                  : "No results"
            color: Theme.textMid
            font.pixelSize: Theme.fontTitle
        }

        Row {
            anchors.horizontalCenter: parent ? parent.horizontalCenter : undefined
            spacing: Theme.space3

            Button {
                text: qsTr("Clear search")
                onClicked: root.clearRequested()
            }

            Button {
                text: qsTr("Search subfolders (soon)")
                enabled: false
            }
        }
    }
}