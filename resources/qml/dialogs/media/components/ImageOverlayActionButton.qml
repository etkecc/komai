// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15
import QtQuick.Controls 2.15

import cc.etke.komai 1.0

AbstractButton {
    id: root

    property string iconSource: ""
    property string labelText: ""
    property bool iconMirror: false
    property color textColor: "white"
    property color hoverIconColor: textColor
    property color hoverTextColor: hoverIconColor
    property color hoverBackgroundColor: Qt.rgba(0, 0, 0, 0.45)

    // Allow buttons anchored to a screen edge to remove top-right rounding only when highlighted.
    property bool flatTopRightCorner: false
    property int iconSize: 24
    property int blockPadding: Komai.paddingMedium
    property int contentSpacing: Komai.paddingSmall

    leftPadding: blockPadding
    rightPadding: blockPadding
    topPadding: blockPadding
    bottomPadding: blockPadding
    implicitWidth: Math.max(76, contentBody.implicitWidth + leftPadding + rightPadding)
    implicitHeight: contentBody.implicitHeight + topPadding + bottomPadding
    hoverEnabled: true
    activeFocusOnTab: true
    focusPolicy: Qt.StrongFocus

    Keys.onEnterPressed: event => {
        event.accepted = true;
        root.clicked();
    }
    Keys.onReturnPressed: event => {
        event.accepted = true;
        root.clicked();
    }

    background: Rectangle {
        radius: Komai.paddingMedium
        color: root.hovered || root.pressed || root.visualFocus ? root.hoverBackgroundColor : "transparent"

        // This overlays the rounded corner so the top-right of just this button stays square
        // when the pointer/keyboard focus is active, matching image-overlay edge positioning.
        Rectangle {
            anchors.top: parent.top
            anchors.right: parent.right
            width: parent.radius
            height: parent.radius
            color: parent.color
            visible: root.flatTopRightCorner && (root.hovered || root.pressed || root.visualFocus)
        }
    }

    contentItem: Item {
        id: contentBody

        implicitWidth: contentColumn.implicitWidth
        implicitHeight: contentColumn.implicitHeight

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            acceptedButtons: Qt.NoButton
        }

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
                        ? "image://colorimage/" + root.iconSource + "?" + (root.hovered || root.visualFocus ? root.hoverIconColor : root.textColor)
                        : ""
                sourceSize.width: width * Screen.devicePixelRatio
                sourceSize.height: height * Screen.devicePixelRatio
            }

            Text {
                anchors.horizontalCenter: parent.horizontalCenter
                color: root.hovered || root.pressed || root.visualFocus ? root.hoverTextColor : root.textColor
                text: root.labelText
                font.bold: true
            }
        }
    }
}
