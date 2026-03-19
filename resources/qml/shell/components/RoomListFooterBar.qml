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
    required property string actionLabel
    required property string actionIcon

    signal actionClicked()

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
        visible: !root.collapsed

        Label {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            text: qsTr("Need more?")
            font.pixelSize: Komai.fontPixelSize
            color: palette.buttonText
            elide: Text.ElideRight
        }

        RoomListActionButton {
            id: actionButton

            readonly property bool hasRoom: actionLabelMetrics.advanceWidth + buttonSize + Komai.paddingSmall < root.width * 0.6

            Layout.rightMargin: Komai.paddingMedium
            buttonSize: Komai.barIconSize
            toolTipText: hasRoom ? "" : root.actionLabel
            iconSource: root.actionIcon
            labelText: root.actionLabel
            showLabel: hasRoom

            TextMetrics {
                id: actionLabelMetrics

                font: Qt.font({
                    "bold": true
                })
                text: actionButton.labelText
            }

            onClicked: root.actionClicked()
        }
    }

    RoomListActionButton {
        id: collapsedButton

        visible: root.collapsed
        anchors.right: parent.right
        anchors.rightMargin: Komai.paddingMedium
        anchors.verticalCenter: parent.verticalCenter
        buttonSize: Komai.barIconSize
        toolTipText: root.actionLabel
        iconSource: root.actionIcon

        onClicked: root.actionClicked()
    }
}
