// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai
import "../../components"

Rectangle {
    id: root

    required property bool collapsed
    required property var timelineRoot

    readonly property bool isAllRooms: !Communities.currentFilterId.startsWith("space:")
        && !Communities.currentFilterId.startsWith("tag:")
    visible: isAllRooms && !collapsed
    height: visible ? Komai.navigationRowHeight : 0
    color: palette.alternateBase

    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        color: Komai.theme.separator
        height: 1
    }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Komai.paddingMedium
        anchors.rightMargin: Komai.paddingMedium
        spacing: Komai.paddingMedium

        Label {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            text: qsTr("Need more?")
            font.pixelSize: Komai.fontPixelSize
            color: palette.buttonText
            elide: Text.ElideRight
        }

        RoomListActionButton {
            id: exploreButton

            readonly property bool hasRoom: exploreLabelMetrics.advanceWidth + buttonSize + Komai.paddingSmall < root.width * 0.6

            buttonSize: Komai.barIconSize
            toolTipText: qsTr("Explore public rooms")
            iconSource: ":/icons/icons/ui/compass-northwest.svg"
            labelText: qsTr("Explore public rooms")
            showLabel: hasRoom

            TextMetrics {
                id: exploreLabelMetrics

                font: Qt.font({
                    "bold": true
                })
                text: exploreButton.labelText
            }

            onClicked: root.timelineRoot.openRoomDirectory()
        }
    }
}
