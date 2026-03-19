// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.5
import cc.etke.komai 1.0


TextMessage {
    id: root

    property bool isStateEvent
    property string stateEventIconSource: ":/icons/icons/ui/state-event.svg"
    readonly property int rawStateEventIconSize: Math.round(Math.floor(stateEventFontMetrics.ascent * 0.9) * 2)
    readonly property int stateEventIconSize: Math.max(12, (rawStateEventIconSize % 2 === 0) ? rawStateEventIconSize : (rawStateEventIconSize + 1))

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
        y: Math.round((stateEventFontMetrics.lineSpacing - height) / 2)
        height: root.stateEventIconSize
        width: root.stateEventIconSize
        fillMode: Image.PreserveAspectFit
        source: root.stateEventIconSource ? ("image://colorimage/" + root.stateEventIconSource + "?" + root.color) : ""
        sourceSize.height: root.stateEventIconSize
        sourceSize.width: root.stateEventIconSize
    }
}
