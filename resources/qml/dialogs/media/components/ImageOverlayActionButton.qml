// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls

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
    property int contentSpacing: Komai.paddingSmall

    // Match the Element Call control bars and the room-header action buttons
    // exactly: the button is iconSize tall and the glyph is inset by the same
    // density-aware padding (wider in Spacious), so the button heights and glyph
    // sizes line up with the fullscreen call OSD and the rest of the app rather
    // than collapsing to a short pill.
    readonly property int buttonPaddingH: (Komai.density !== Settings.Density.Spacious) ? Komai.paddingSmall : Komai.paddingMedium
    property int iconSize: Math.max(14, Komai.iconSize - 2 * buttonPaddingH)

    leftPadding: buttonPaddingH
    rightPadding: buttonPaddingH
    topPadding: 0
    bottomPadding: 0
    implicitWidth: contentBody.implicitWidth + leftPadding + rightPadding
    implicitHeight: Komai.iconSize
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

        implicitWidth: contentRow.implicitWidth
        implicitHeight: contentRow.implicitHeight

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            acceptedButtons: Qt.NoButton
        }

        Row {
            id: contentRow

            spacing: root.contentSpacing
            anchors.centerIn: parent

            Image {
                width: root.iconSize
                height: root.iconSize
                anchors.verticalCenter: parent.verticalCenter
                mirror: root.iconMirror
                source: root.iconSource !== ""
                        ? "image://colorimage/" + root.iconSource + "?" + (root.hovered || root.visualFocus ? root.hoverIconColor : root.textColor)
                        : ""
                sourceSize.width: width * Screen.devicePixelRatio
                sourceSize.height: height * Screen.devicePixelRatio
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                visible: root.labelText !== ""
                color: root.hovered || root.pressed || root.visualFocus ? root.hoverTextColor : root.textColor
                text: root.labelText
                font.bold: true
            }
        }
    }
}
