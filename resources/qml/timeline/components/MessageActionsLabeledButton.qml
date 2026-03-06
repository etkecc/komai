// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai

AbstractButton {
    id: button

    required property string labelText
    required property string image
    property color buttonTextColor: palette.buttonText
    property color hoverIconColor: palette.brightText
    property color labelTextColor: palette.text
    property color hoverTextColor: palette.brightText
    property color hoverBackgroundColor: palette.dark
    property int buttonHeight: 0
    property int iconSize: 32
    property int contentHorizontalPadding: Komai.paddingSmall
    property int contentVerticalPadding: Komai.paddingSmall
    property bool mirrorIcon: false
    property string toolTipText: labelText
    readonly property bool hasLabel: labelText.length > 0
    readonly property bool activeState: hovered || pressed || visualFocus
    readonly property int labelHeight: hasLabel ? label.implicitHeight : 0

    ToolTip.delay: Komai.tooltipDelay
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
    implicitHeight: Math.max(buttonHeight, Math.max(iconSize, labelHeight) + topPadding + bottomPadding)
    implicitWidth: leftPadding + rightPadding + iconSize + (hasLabel ? (Komai.paddingSmall + label.implicitWidth) : 0)
    Layout.preferredHeight: implicitHeight
    Layout.preferredWidth: implicitWidth

    background: Rectangle {
        radius: Komai.paddingSmall
        color: button.activeState ? button.hoverBackgroundColor : "transparent"
    }

    contentItem: RowLayout {
        anchors.fill: parent
        anchors.leftMargin: button.leftPadding
        anchors.rightMargin: button.rightPadding
        anchors.topMargin: button.topPadding
        anchors.bottomMargin: button.bottomPadding
        spacing: Komai.paddingSmall

        Image {
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredHeight: button.iconSize
            Layout.preferredWidth: button.iconSize
            source: button.image !== "" ? ("image://colorimage/" + button.image + "?" + (button.activeState ? button.hoverIconColor : button.buttonTextColor)) : ""
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
            color: button.activeState ? button.hoverTextColor : button.labelTextColor
            font.bold: true
            text: button.labelText
            visible: button.hasLabel
        }
    }

    KomaiCursorShape {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
    }
}
