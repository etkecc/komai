// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.5
import cc.etke.komai 1.0


TextMessage {
    id: root

    property bool isStateEvent
    readonly property int stateEventIconSize: Math.max(12, Math.round(stateEventFontMetrics.height * 0.85))

    font.italic: true
    color: palette.buttonText
    font.pointSize: isStateEvent? 0.95*Settings.uiFontSizePt : Settings.uiFontSizePt
    leftPadding: isStateEvent ? (stateEventIconSize + Komai.paddingSmall) : 0

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
