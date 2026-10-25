import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Effects

import qg

// Reel panel (DESIGN.md §10): the right-side ordered video list of the folder
// being played — the SAME ordered source as Previous/Next, folders excluded.
// Header: folder name, active sort, "7 / 42", filter chips. Rows: index, name,
// duration, state badges and a thin partial-watch progress bar; the current item
// is anchored (mint marker + bold) and scrolled into view. Footer: ‹ prev,
// favorite toggle, next ›.
// Presentation only: every interaction forwards an intent; the model supplies
// raw duration/size which are formatted here through the `Format` singleton.

Rectangle {
    id: reel
    objectName: "reelPanel"

    property var reelModel: null
    property string folderName: ""
    property string sortLabel: ""
    property string positionLabel: ""
    property string filterLabel: "All"
    property bool canGoPrevious: false
    property bool canGoNext: false

    signal previousRequested()
    signal nextRequested()
    signal favoriteToggled()
    signal filterChanged(string filter)
    signal itemActivated(int row)
    signal closeRequested()

    // High-DPI scaled dimensions
    readonly property int scaledWidth: Theme.scale(240)
    readonly property int scaledHeaderHeight: Theme.scale(48)
    readonly property int scaledRowHeight: Theme.scale(38)
    readonly property int scaledFooterHeight: Theme.scale(32)
    readonly property int scaledMarkerWidth: Theme.scale(3)
    readonly property int scaledProgressHeight: Theme.scale(2)

    width: scaledWidth
    color: Theme.surface1

    readonly property var filterLabels: ["All", "Favorites", "Unwatched"]

    // Testable forwarding helpers (Quick Test cannot click a Repeater delegate
    // reliably; the emit contract is exercised directly).
    function selectFilter(label) {
        filterChanged(label)
    }

    function activateRow(row) {
        itemActivated(row)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // Header ------------------------------------------------------------
        Rectangle {
            Layout.fillWidth: true
            height: scaledHeaderHeight
            color: Theme.surface2

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: Theme.space2
                spacing: 2

                RowLayout {
                    Layout.fillWidth: true

                    Text {
                        text: reel.folderName
                        color: Theme.textHi
                        font.bold: true
                        font.pixelSize: Theme.fontBody
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    Text {
                        objectName: "reelPosition"
                        text: reel.positionLabel
                        color: Theme.textMid
                        font.pixelSize: Theme.fontMicro
                    }

                    ToolButton {
                        objectName: "reelClose"
                        contentItem: Icon {
                            name: "close"
                            color: Theme.textHi
                            size: 16
                        }
                        onClicked: reel.closeRequested()
                        Accessible.name: "Close the reel"
                    }
                }

                Text {
                    objectName: "reelSortLabel"
                    text: reel.sortLabel.length > 0
                          ? qsTr("sorted: %1").arg(reel.sortLabel) : ""
                    color: Theme.textLow
                    font.pixelSize: Theme.fontMicro
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
            }
        }

        // Filter chips ------------------------------------------------------
        Row {
            objectName: "reelFilters"
            Layout.fillWidth: true
            Layout.margins: Theme.space1
            spacing: Theme.space1

            Repeater {
                model: reel.filterLabels
                delegate: Rectangle {
                    objectName: "reelFilterChip"
                    radius: Theme.radiusChip
                    color: modelData === reel.filterLabel ? Theme.surface3 : "transparent"
                    implicitWidth: labelText.implicitWidth + Theme.space2
                    implicitHeight: 20

                    Text {
                        id: labelText
                        anchors.centerIn: parent
                        text: modelData
                        color: modelData === reel.filterLabel ? Theme.textHi : Theme.textMid
                        font.pixelSize: Theme.fontMicro
                    }

                    MouseArea {
                        anchors.fill: parent
                        onClicked: reel.selectFilter(modelData)
                    }
                }
            }
        }

        // Video list --------------------------------------------------------
        ListView {
            id: reelList
            objectName: "reelList"
            Layout.fillWidth: true
            Layout.fillHeight: true
            model: reel.reelModel
            clip: true
            currentIndex: (reel.reelModel && reel.reelModel.currentIndex !== undefined)
                          ? reel.reelModel.currentIndex : -1

            // Anchor the current item: scroll it into view on open and on media
            // change (DESIGN.md §10.2).
            onCurrentIndexChanged: if (currentIndex >= 0) positionViewAtIndex(currentIndex, ListView.Contain)
            Component.onCompleted: if (currentIndex >= 0) positionViewAtIndex(currentIndex, ListView.Contain)

            delegate: Rectangle {
                objectName: "reelRow"
                width: reelList.width
                height: scaledRowHeight
                color: model.reelIsCurrent ? Theme.surface3 : "transparent"

                // Mint left marker for the current item.
                Rectangle {
                    width: scaledMarkerWidth
                    height: parent.height
                    color: Theme.play
                    visible: model.reelIsCurrent
                }

                RowLayout {
                    anchors.fill: parent
                    anchors.margins: Theme.space1
                    spacing: Theme.space1

                    Text {
                        text: model.reelIndex
                        color: Theme.textLow
                        font.pixelSize: Theme.fontMicro
                        Layout.preferredWidth: 20
                    }

                    Text {
                        text: model.reelName
                        color: model.reelIsCurrent ? Theme.play : Theme.textHi
                        font.pixelSize: Theme.fontBody
                        font.bold: model.reelIsCurrent
                        elide: Text.ElideRight
                        Layout.fillWidth: true
                    }

                    Text {
                        text: Format.duration(model.reelDurationMs)
                        color: Theme.textLow
                        font.pixelSize: Theme.fontMicro
                    }

                    Icon {
                        visible: model.reelFavorite
                        name: "favorite"
                        color: Theme.favorite
                        size: Theme.fontMicro
                    }

                    Icon {
                        visible: model.reelWatched
                        name: "watched"
                        color: Theme.watched
                        size: Theme.fontMicro
                    }
                }

                // Thin partial-watch bar under the row (DESIGN.md §10.2).
                Rectangle {
                    id: progressBar
                    readonly property bool hasProgress: model.reelProgress !== undefined
                                                        && model.reelProgress >= 0
                    anchors.bottom: parent.bottom
                    anchors.left: parent.left
                    height: scaledProgressHeight
                    visible: hasProgress
                    width: hasProgress ? parent.width * model.reelProgress : 0
                    color: Theme.signal
                }

                MouseArea {
                    anchors.fill: parent
                    onClicked: reel.activateRow(index)
                }
            }
        }

        // Footer: prev / next + favorite toggle ------------------------------
        Rectangle {
            Layout.fillWidth: true
            height: scaledFooterHeight
            color: Theme.surface2

            RowLayout {
                anchors.centerIn: parent
                spacing: Theme.space3

                ToolButton {
                    objectName: "reelPrev"
                    contentItem: Row {
                        spacing: Theme.space1
                        Icon {
                            name: "prev"
                            color: Theme.textHi
                            size: Theme.scale(14)
                        }
                        Text {
                            text: qsTr("prev")
                            color: Theme.textHi
                            font.pixelSize: Theme.fontBody
                        }
                    }
                    enabled: reel.canGoPrevious
                    onClicked: reel.previousRequested()
                }

                ToolButton {
                    objectName: "reelFavorite"
                    contentItem: Icon {
                        name: "favorite"
                        color: Theme.favorite
                        size: Theme.scale(16)
                    }
                    onClicked: reel.favoriteToggled()
                    Accessible.name: "Toggle favorite for the current item"
                }

                ToolButton {
                    objectName: "reelNext"
                    contentItem: Row {
                        spacing: Theme.space1
                        Text {
                            text: qsTr("next")
                            color: Theme.textHi
                            font.pixelSize: Theme.fontBody
                        }
                        Icon {
                            name: "next"
                            color: Theme.textHi
                            size: Theme.scale(14)
                        }
                    }
                    enabled: reel.canGoNext
                    onClicked: reel.nextRequested()
                }
            }
        }
    }
}