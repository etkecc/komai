// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../components"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import cc.etke.komai

AbstractButton {
    id: root

    property int buttonSize: Komai.barIconSize
    property int buttonPaddingH: Komai.uiLayoutCompactMode ? Komai.paddingSmall : Komai.paddingMedium
    property int buttonPaddingV: 0
    property string image: ""
    property string toolTipText: ""
    property color buttonTextColor: palette.buttonText
    property real toolTipAnchorX: width / 2
    readonly property int iconSize: Math.max(14, buttonSize - 2 * buttonPaddingH)
    readonly property bool activeState: hovered || pressed || visualFocus
    readonly property color actionTextColor: activeState ? palette.brightText : buttonTextColor

    function isActivationKey(event) {
        if (!event)
            return false;

        const modifiers = Number(event.modifiers);
        if ((modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier)) !== 0)
            return false;

        return event.key === Qt.Key_Return
            || event.key === Qt.Key_Enter
            || event.key === Qt.Key_Space;
    }

    font.pointSize: Settings.uiFontSizePt
    implicitHeight: buttonSize
    implicitWidth: buttonSize
    Layout.preferredHeight: buttonSize
    Layout.preferredWidth: buttonSize
    activeFocusOnTab: visible && enabled
    focusPolicy: Qt.StrongFocus
    hoverEnabled: true
    Keys.priority: Keys.BeforeItem
    Keys.onPressed: event => {
        if (!root.enabled || !root.isActivationKey(event))
            return;

        root.clicked();
        event.accepted = true;
    }

    leftPadding: buttonPaddingH
    rightPadding: buttonPaddingH
    topPadding: buttonPaddingV
    bottomPadding: buttonPaddingV

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

    HoverHandler {
        onPointChanged: if (hovered)
            root.toolTipAnchorX = point.position.x
    }

    background: Rectangle {
        radius: Komai.paddingSmall
        color: root.activeState ? palette.dark : "transparent"
    }

    contentItem: Item {
        Image {
            anchors.centerIn: parent
            height: root.iconSize
            source: root.image !== "" ? ("image://colorimage/" + root.image + "?" + root.actionTextColor) : ""
            sourceSize.height: root.iconSize
            sourceSize.width: root.iconSize
            width: root.iconSize
        }
    }

    KomaiCursorShape {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
    }
}
