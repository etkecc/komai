// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai

AbstractButton {
    id: root

    required property int buttonSize
    required property string toolTipText
    required property string iconSource
    property string labelText: ""
    property bool showLabel: false
    readonly property bool hasLabel: showLabel && labelText.length > 0
    property int buttonPaddingH: Komai.uiLayoutCompactMode ? Komai.paddingSmall : Komai.paddingMedium
    property int buttonPaddingV: 0
    readonly property int iconSize: Math.max(14, buttonSize - 2 * buttonPaddingH)
    readonly property bool activeState: hovered || pressed || visualFocus
    readonly property color actionTextColor: activeState ? palette.brightText : palette.buttonText
    readonly property color actionLabelColor: activeState ? palette.brightText : palette.text

    ToolTip.delay: Komai.tooltipDelay
    ToolTip.text: toolTipText
    ToolTip.visible: hovered
    font.pointSize: Settings.uiFontSizePt
    implicitHeight: buttonSize
    implicitWidth: buttonSize + (hasLabel ? (Komai.paddingSmall + actionLabel.implicitWidth) : 0)
    Layout.preferredHeight: buttonSize
    Layout.preferredWidth: implicitWidth
    hoverEnabled: true
    leftPadding: buttonPaddingH
    rightPadding: buttonPaddingH
    topPadding: buttonPaddingV
    bottomPadding: buttonPaddingV

    background: Rectangle {
        radius: Komai.paddingSmall
        color: root.activeState ? palette.dark : "transparent"
    }

    contentItem: RowLayout {
        anchors.fill: parent
        anchors.leftMargin: root.leftPadding
        anchors.rightMargin: root.rightPadding
        anchors.topMargin: root.topPadding
        anchors.bottomMargin: root.bottomPadding
        spacing: Komai.paddingSmall

        Image {
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredHeight: root.iconSize
            Layout.preferredWidth: root.iconSize
            source: "image://colorimage/" + root.iconSource + "?" + root.actionTextColor
            sourceSize.height: root.iconSize
            sourceSize.width: root.iconSize
        }
        Label {
            id: actionLabel

            Layout.alignment: Qt.AlignVCenter
            color: root.actionLabelColor
            font.bold: true
            text: root.labelText
            visible: root.hasLabel
        }
    }

    KomaiCursorShape {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
    }
}
