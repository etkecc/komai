// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15
import QtQuick.Controls 2.15
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
    implicitHeight: notificationBubbleText.height + Komai.paddingMedium
    implicitWidth: Math.max(notificationBubbleText.width, height)
    radius: height / 2
    color: hasLoudNotification ? Komai.theme.attention : bubbleBackgroundColor
    ToolTip.text: notificationCount
    ToolTip.delay: Komai.tooltipDelay
    ToolTip.visible: notificationBubbleHover.hovered && (notificationCount > 9999)

    Label {
        id: notificationBubbleText

        anchors.centerIn: bubbleRoot
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        width: Math.max(implicitWidth + Komai.paddingMedium, bubbleRoot.height)
        font.bold: true
        font.pixelSize: bubbleRoot.baseFontPixelSize * 0.8
        color: bubbleRoot.hasLoudNotification ? "white" : bubbleRoot.bubbleTextColor
        text: bubbleRoot.notificationCount > 9999 ? "9999+" : bubbleRoot.notificationCount

        HoverHandler {
            id: notificationBubbleHover
        }

    }

}
