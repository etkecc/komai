// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import cc.etke.komai
import "../../components"

AbstractButton {
    id: root

    required property int buttonSize
    required property string toolTipText
    required property string iconSource
    property string labelText: ""
    property bool showLabel: false
    property real toolTipAnchorX: width / 2
    readonly property bool hasLabel: showLabel && labelText.length > 0
    property int buttonPaddingH: (Komai.density !== Settings.Density.Spacious) ? Komai.paddingSmall : Komai.paddingMedium
    property int buttonPaddingV: 0
    readonly property int iconSize: Math.max(14, buttonSize - 2 * buttonPaddingH)
    readonly property bool activeState: hovered || pressed || visualFocus
    readonly property color actionTextColor: activeState ? palette.brightText : palette.buttonText
    readonly property color actionLabelColor: activeState ? palette.brightText : palette.text

    TextMetrics {
        id: toolTipMetrics

        font: root.font
        text: root.toolTipText
    }

    KomaiToolTip {
        anchorItem: root
        anchorX: root.toolTipAnchorX
        anchorY: root.height
        gapX: Komai.paddingMedium
        gapY: Komai.paddingMedium
        text: root.toolTipText
        delay: 0
        requestedVisible: root.hovered && root.toolTipText.length > 0
        width: Math.min(toolTipMetrics.advanceWidth + leftPadding + rightPadding,
                        (root.Window.window ? root.Window.window.width : 500) * 0.5)
    }

    font.pointSize: Settings.uiFontSizePt
    implicitHeight: buttonSize
    implicitWidth: buttonSize + (hasLabel ? (Komai.paddingSmall + actionLabel.implicitWidth) : 0)
    Layout.preferredHeight: buttonSize
    Layout.preferredWidth: implicitWidth
    activeFocusOnTab: visible && enabled
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true
    leftPadding: buttonPaddingH
    rightPadding: buttonPaddingH
    topPadding: buttonPaddingV
    bottomPadding: buttonPaddingV

    HoverHandler {
        onPointChanged: if (hovered)
            root.toolTipAnchorX = point.position.x
    }

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
