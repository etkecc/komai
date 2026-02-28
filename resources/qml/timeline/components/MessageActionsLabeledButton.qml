// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import im.nheko

AbstractButton {
    id: button

    required property string labelText
    required property string image
    property color buttonTextColor: palette.buttonText
    property color hoverIconColor: palette.highlight
    property color hoverTextColor: hoverIconColor
    property color hoverBackgroundColor: Qt.rgba(0, 0, 0, 0.45)
    property int iconSize: 32
    property int contentHorizontalPadding: Nheko.paddingSmall
    property int contentVerticalPadding: Nheko.paddingSmall
    property bool mirrorIcon: false
    property string toolTipText: labelText
    readonly property bool hasLabel: labelText.length > 0

    ToolTip.delay: Nheko.tooltipDelay
    ToolTip.text: toolTipText
    ToolTip.visible: hovered
    hoverEnabled: true
    activeFocusOnTab: true
    focusPolicy: Qt.StrongFocus
    leftPadding: contentHorizontalPadding
    rightPadding: contentHorizontalPadding
    topPadding: contentVerticalPadding
    bottomPadding: contentVerticalPadding
    leftInset: 0
    rightInset: 0
    topInset: 0
    bottomInset: 0
    implicitHeight: iconSize + topPadding + bottomPadding
    implicitWidth: leftPadding + rightPadding + iconSize + (hasLabel ? (Nheko.paddingSmall + label.implicitWidth) : 0)
    Layout.preferredHeight: implicitHeight
    Layout.preferredWidth: implicitWidth

    background: Rectangle {
        radius: Nheko.paddingMedium
        color: button.hovered || button.pressed || button.visualFocus ? button.hoverBackgroundColor : "transparent"
    }

    contentItem: RowLayout {
        anchors.fill: parent
        anchors.leftMargin: button.leftPadding
        anchors.rightMargin: button.rightPadding
        anchors.topMargin: button.topPadding
        anchors.bottomMargin: button.bottomPadding
        spacing: Nheko.paddingSmall

        Image {
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredHeight: button.iconSize
            Layout.preferredWidth: button.iconSize
            source: button.image !== "" ? ("image://colorimage/" + button.image + "?" + button.buttonTextColor) : ""
            sourceSize.height: button.iconSize
            sourceSize.width: button.iconSize

            transform: Scale {
                origin.x: button.iconSize / 2
                xScale: button.mirrorIcon ? -1 : 1
            }
        }
        Label {
            id: label

            Layout.alignment: Qt.AlignVCenter
            color: button.buttonTextColor
            font.bold: true
            text: button.labelText
            visible: button.hasLabel
        }
    }

    NhekoCursorShape {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
    }
}
