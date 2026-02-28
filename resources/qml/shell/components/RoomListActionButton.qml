// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import im.nheko

AbstractButton {
    id: root

    required property int buttonSize
    required property string toolTipText
    required property string iconSource
    property string labelText: ""
    property bool showLabel: false
    readonly property bool hasLabel: showLabel && labelText.length > 0
    property int buttonPaddingH: Nheko.uiLayoutCompactMode ? Nheko.paddingSmall : Nheko.paddingMedium
    property int buttonPaddingV: 0
    readonly property int iconSize: Math.max(14, buttonSize - 2 * buttonPaddingH)

    ToolTip.delay: Nheko.tooltipDelay
    ToolTip.text: toolTipText
    ToolTip.visible: hovered
    implicitHeight: buttonSize
    implicitWidth: buttonSize + (hasLabel ? (Nheko.paddingSmall + actionLabel.implicitWidth) : 0)
    Layout.preferredHeight: buttonSize
    Layout.preferredWidth: implicitWidth
    hoverEnabled: true
    leftPadding: buttonPaddingH
    rightPadding: buttonPaddingH
    topPadding: buttonPaddingV
    bottomPadding: buttonPaddingV

    background: Rectangle {
        radius: Nheko.paddingSmall
        color: root.hovered ? Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, 0.12) : "transparent"
    }

    contentItem: RowLayout {
        anchors.fill: parent
        anchors.leftMargin: root.leftPadding
        anchors.rightMargin: root.rightPadding
        anchors.topMargin: root.topPadding
        anchors.bottomMargin: root.bottomPadding
        spacing: Nheko.paddingSmall

        Image {
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredHeight: root.iconSize
            Layout.preferredWidth: root.iconSize
            source: "image://colorimage/" + root.iconSource + "?" + (root.hovered ? palette.highlight : palette.buttonText)
            sourceSize.height: root.iconSize
            sourceSize.width: root.iconSize
        }
        Label {
            id: actionLabel

            Layout.alignment: Qt.AlignVCenter
            color: palette.text
            font.bold: true
            text: root.labelText
            visible: root.hasLabel
        }
    }

    NhekoCursorShape {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
    }
}
