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
    id: button

    required property var chatRoot
    property int buttonHeight: Komai.listIconSize
    property int buttonPaddingH: Komai.uiLayoutCompactMode ? Komai.paddingSmall : Komai.paddingMedium
    property int buttonPaddingV: 0
    property string image: ""
    property string labelText: ""
    property bool showLabel: false
    property bool alwaysShowToolTip: true
    property string toolTipText: labelText
    property real toolTipAnchorX: width / 2
    property bool mirrorIcon: false
    readonly property bool hasLabel: showLabel && labelText.length > 0
    readonly property int iconSize: Math.max(14, buttonHeight - 2 * buttonPaddingH)
    readonly property bool activeState: enabled && (hovered || pressed || visualFocus)
    readonly property color actionTextColor: !enabled ? palette.buttonText : (activeState ? palette.brightText : palette.buttonText)
    readonly property color actionLabelColor: !enabled ? palette.buttonText : (activeState ? palette.brightText : palette.text)

    function handleWalkModeEvent(event) {
        if (!chatRoot || typeof chatRoot.handleWalkModeKey !== "function")
            return false;

        return chatRoot.handleWalkModeKey(event);
    }

    Layout.alignment: Qt.AlignVCenter
    Layout.preferredHeight: implicitHeight
    Layout.preferredWidth: implicitWidth
    activeFocusOnTab: true
    focusPolicy: Qt.StrongFocus
    font.pointSize: Settings.uiFontSizePt
    hoverEnabled: true
    Keys.priority: Keys.BeforeItem
    Keys.onPressed: event => button.handleWalkModeEvent(event)
    leftPadding: buttonPaddingH
    rightPadding: buttonPaddingH
    topPadding: buttonPaddingV
    bottomPadding: buttonPaddingV
    implicitHeight: buttonHeight
    implicitWidth: buttonHeight + (hasLabel ? (Komai.paddingSmall + actionLabel.implicitWidth) : 0)

    TextMetrics {
        id: toolTipMetrics

        font: button.font
        text: button.toolTipText
    }

    KomaiToolTip {
        anchorItem: button
        anchorX: button.toolTipAnchorX
        anchorY: button.height
        gapX: Komai.paddingMedium
        gapY: Komai.paddingMedium
        text: button.toolTipText
        delay: 0
        requestedVisible: button.hovered
            && button.toolTipText.length > 0
            && (button.alwaysShowToolTip || !button.hasLabel)
        width: Math.min(toolTipMetrics.advanceWidth + leftPadding + rightPadding,
                        (button.Window.window ? button.Window.window.width : 500) * 0.5)
    }

    HoverHandler {
        onPointChanged: if (hovered)
            button.toolTipAnchorX = point.position.x
    }

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

            transform: Scale {
                origin.x: button.iconSize / 2
                xScale: button.mirrorIcon ? -1 : 1
            }
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
        cursorShape: button.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
    }
}
