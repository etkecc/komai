// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import cc.etke.komai 1.0

Rectangle {
    id: bubbleRoot

    required property int notificationCount
    required property bool hasLoudNotification
    required property color bubbleBackgroundColor
    required property color bubbleTextColor
    readonly property real baseFontPixelSize: Komai.fontPixelSize
    property bool mayBeVisible: true
    property alias font: notificationBubbleText.font
    baselineOffset: notificationBubbleText.baseline - bubbleRoot.top

    visible: mayBeVisible && notificationCount > 0
    implicitHeight: notificationBubbleText.height + Komai.paddingSmall
    implicitWidth: Math.max(notificationBubbleText.width, height)
    radius: height / 8
    color: hasLoudNotification ? Komai.theme.attention : bubbleBackgroundColor

    KomaiToolTip {
        anchorItem: bubbleRoot
        anchorX: bubbleRoot.width / 2
        anchorY: bubbleRoot.height
        gapX: Komai.paddingMedium
        gapY: Komai.paddingMedium
        text: String(bubbleRoot.notificationCount)
        delay: Komai.tooltipDelay
        requestedVisible: notificationBubbleHover.hovered && bubbleRoot.notificationCount > 9999
    }

    Label {
        id: notificationBubbleText

        anchors.centerIn: bubbleRoot
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        width: Math.max(implicitWidth + Komai.paddingSmall, bubbleRoot.height)
        font.bold: true
        font.pixelSize: bubbleRoot.baseFontPixelSize * 0.8
        color: bubbleRoot.hasLoudNotification ? "white" : bubbleRoot.bubbleTextColor
        text: bubbleRoot.notificationCount > 9999 ? "9999+" : bubbleRoot.notificationCount

        HoverHandler {
            id: notificationBubbleHover
        }

    }

}
