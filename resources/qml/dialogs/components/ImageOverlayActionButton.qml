// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15
import QtQuick.Controls 2.15

import im.nheko 1.0

AbstractButton {
    id: root

    property string iconSource: ""
    property string labelText: ""
    property bool iconMirror: false
    property color textColor: "white"
    property color hoverIconColor: textColor
    property color hoverTextColor: hoverIconColor
    property color hoverBackgroundColor: Qt.rgba(0, 0, 0, 0.45)
    property int iconSize: 24
    property int blockPadding: Nheko.paddingMedium
    property int contentSpacing: Nheko.paddingSmall

    leftPadding: blockPadding
    rightPadding: blockPadding
    topPadding: blockPadding
    bottomPadding: blockPadding
    implicitWidth: Math.max(76, contentBody.implicitWidth + leftPadding + rightPadding)
    implicitHeight: contentBody.implicitHeight + topPadding + bottomPadding
    hoverEnabled: true

    background: Rectangle {
        radius: Nheko.paddingMedium
        color: root.hovered || root.pressed ? root.hoverBackgroundColor : "transparent"
    }

    contentItem: Item {
        id: contentBody

        implicitWidth: contentColumn.implicitWidth
        implicitHeight: contentColumn.implicitHeight

        Column {
            id: contentColumn

            spacing: root.contentSpacing
            anchors.centerIn: parent

            Image {
                width: root.iconSize
                height: root.iconSize
                anchors.horizontalCenter: parent.horizontalCenter
                mirror: root.iconMirror
                source: root.iconSource !== ""
                        ? "image://colorimage/" + root.iconSource + "?" + (root.hovered ? root.hoverIconColor : root.textColor)
                        : ""
                sourceSize.width: width * Screen.devicePixelRatio
                sourceSize.height: height * Screen.devicePixelRatio
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                color: root.hovered || root.pressed ? root.hoverTextColor : root.textColor
                text: root.labelText
                font.bold: true
            }
        }
    }
}
