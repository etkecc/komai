// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai

AbstractButton {
    id: button

    required property var topBarRef
    required property int column
    property int row: 1
    property string image: ""
    property string labelText: ""
    property bool showLabel: false
    property bool alwaysShowToolTip: false
    property string toolTipText: labelText
    readonly property bool hasLabel: showLabel && labelText.length > 0
    readonly property int iconSize: Math.max(14, topBarRef.topBarAvatarSize - 2 * topBarRef.buttonPaddingH)
    readonly property bool activeState: hovered || pressed || visualFocus
    readonly property color actionTextColor: activeState ? palette.brightText : palette.buttonText
    readonly property color actionLabelColor: activeState ? palette.brightText : palette.text

    Layout.alignment: Qt.AlignVCenter
    Layout.column: column
    Layout.preferredHeight: topBarRef.topBarAvatarSize
    Layout.preferredWidth: implicitWidth
    Layout.row: row
    hoverEnabled: true
    leftPadding: topBarRef.buttonPaddingH
    rightPadding: topBarRef.buttonPaddingH
    topPadding: topBarRef.buttonPaddingV
    bottomPadding: topBarRef.buttonPaddingV
    implicitHeight: topBarRef.topBarAvatarSize
    implicitWidth: topBarRef.topBarAvatarSize + (hasLabel ? (Komai.paddingSmall + actionLabel.implicitWidth) : 0)
    ToolTip.delay: Komai.tooltipDelay
    ToolTip.text: toolTipText
    ToolTip.visible: hovered && (alwaysShowToolTip || !hasLabel)

    background: Rectangle {
        radius: Komai.paddingSmall
        color: button.activeState ? palette.dark : "transparent"
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
            source: button.image !== "" ? ("image://colorimage/" + button.image + "?" + button.actionTextColor) : ""
            sourceSize.height: button.iconSize
            sourceSize.width: button.iconSize
        }
        Label {
            id: actionLabel

            Layout.alignment: Qt.AlignVCenter
            color: button.actionLabelColor
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
