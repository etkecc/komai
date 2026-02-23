// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.5
import im.nheko 1.0


TextMessage {
    id: root

    property bool isStateEvent
    readonly property int stateEventIconSize: Math.max(12, Math.round(stateEventFontMetrics.height * 0.85))

    font.italic: true
    color: palette.buttonText
    font.pointSize: isStateEvent? 0.95*Settings.fontSize : Settings.fontSize
    horizontalAlignment: isStateEvent ? Text.AlignLeft : undefined
    leftPadding: isStateEvent ? (stateEventIconSize + Nheko.paddingSmall) : 0

    FontMetrics {
        id: stateEventFontMetrics
        font: root.font
    }

    Image {
        id: stateEventIcon

        visible: root.isStateEvent
        anchors.left: root.left
        anchors.top: root.top
        anchors.topMargin: Math.max(0, Math.round((stateEventFontMetrics.lineSpacing - height) / 2))
        height: root.stateEventIconSize
        width: root.stateEventIconSize
        fillMode: Image.PreserveAspectFit
        source: "image://colorimage/:/icons/icons/ui/state-event.svg?" + root.color
    }
}
