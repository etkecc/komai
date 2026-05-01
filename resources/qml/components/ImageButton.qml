// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import "../ui"
import QtQuick
import QtQuick.Controls
import QtQuick.Window
import cc.etke.komai // for cursor shape

AbstractButton {
    id: button

    property color buttonTextColor: palette.buttonText
    property bool changeColorOnHover: true
    property int cursor: Qt.PointingHandCursor
    property color highlightColor: palette.highlight
    property string image: undefined
    property bool ripple: true
    property bool hoverPulse: false
    property string toolTipText: ""
    property bool toolTipVisible: hovered && toolTipText.length > 0
    property int toolTipDelay: 0
    property real toolTipAnchorX: width / 2

    // Default to NoFocus so dense icon-button rows (toolbars, message
    // actions) don't trap Tab navigation. Callers can override to
    // Qt.StrongFocus per-instance for buttons that genuinely deserve a
    // tab stop (e.g. password show/hide).
    focusPolicy: Qt.NoFocus
    font.pointSize: Settings.uiFontSizePt
    // Default Accessible.name to the tooltip so screen readers always have
    // some name to announce, even for callers that forget to set one.
    Accessible.name: button.toolTipText
    height: 16
    width: 16

    TextMetrics {
        id: toolTipMetrics

        font: button.font
        text: button.toolTipText
    }

    KomaiToolTip {
        anchorItem: button
        anchorX: button.toolTipAnchorX
        anchorY: 0
        gapX: Komai.paddingMedium
        gapY: Komai.paddingMedium
        text: button.toolTipText
        delay: button.toolTipDelay
        requestedVisible: button.toolTipVisible
        width: Math.min(toolTipMetrics.advanceWidth + leftPadding + rightPadding,
                        (button.Window.window ? button.Window.window.width : 500) * 0.5)
    }

    Image {
        id: buttonImg

        // Workaround, can't get icon.source working for now...
        anchors.fill: parent
        anchors.leftMargin: button.leftPadding
        anchors.rightMargin: button.rightPadding
        anchors.topMargin: button.topPadding
        anchors.bottomMargin: button.bottomPadding
        fillMode: Image.PreserveAspectFit
        source: button.image != "" ? ("image://colorimage/" + button.image + "?" + ((button.hovered && button.changeColorOnHover) ? button.highlightColor : button.buttonTextColor)) : ""
        sourceSize.height: buttonImg.height
        sourceSize.width: buttonImg.width
    }
    KomaiCursorShape {
        id: mouseArea

        anchors.fill: parent
        cursorShape: button.cursor
    }
    Ripple {
        color: Qt.rgba(button.buttonTextColor.r, button.buttonTextColor.g, button.buttonTextColor.b, 0.5)
        enabled: button.ripple
    }
    HoverPulseAnimation {
        id: hoverPulseAnim

        targetItem: button
    }
    onHoveredChanged: {
        if (hovered && hoverPulse)
            hoverPulseAnim.pulse();
    }
    HoverHandler {
        onPointChanged: if (hovered)
            button.toolTipAnchorX = point.position.x
    }
}
