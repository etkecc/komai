// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import im.nheko

AbstractButton {
    id: button

    required property var topBarRef
    required property int column
    property int row: 1
    property string image: ""
    property string labelText: ""
    property bool showLabel: false
    property string toolTipText: labelText
    readonly property bool hasLabel: showLabel && labelText.length > 0
    readonly property int iconSize: Math.max(14, topBarRef.topBarAvatarSize - 2 * topBarRef.buttonPaddingH)

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
    implicitWidth: topBarRef.topBarAvatarSize + (hasLabel ? (Nheko.paddingSmall + actionLabel.implicitWidth) : 0)
    ToolTip.delay: Nheko.tooltipDelay
    ToolTip.text: toolTipText
    ToolTip.visible: hovered && !hasLabel

    background: Rectangle {
        radius: Nheko.paddingSmall
        color: button.hovered ? Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, 0.12) : "transparent"
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
            source: button.image !== "" ? ("image://colorimage/" + button.image + "?" + (button.hovered ? palette.highlight : palette.buttonText)) : ""
            sourceSize.height: button.iconSize
            sourceSize.width: button.iconSize
        }
        Label {
            id: actionLabel

            Layout.alignment: Qt.AlignVCenter
            color: palette.text
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
