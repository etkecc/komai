// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import cc.etke.komai 1.0


TextMessage {
    id: root

    property bool isStateEvent
    // When true, the inline state-event icon sits at the text's trailing edge.
    // Used for right-aligned rows so the icon moves to the side opposite the
    // metadata column instead of crowding it.
    property bool stateEventIconOnRight: false
    property string stateEventIconSource: ":/icons/icons/ui/state-event.svg"
    property string stateEventIconColorCategory: "neutral"
    readonly property int rawStateEventIconSize: Math.round(Math.floor(stateEventFontMetrics.ascent * 0.9) * 2)
    readonly property int stateEventIconSize: Math.max(12, (rawStateEventIconSize % 2 === 0) ? rawStateEventIconSize : (rawStateEventIconSize + 1))
    readonly property color stateEventIconColor: {
        switch (stateEventIconColorCategory) {
        case "positive": return Komai.theme.success;
        case "negative": return Komai.theme.error;
        case "cautious": return Komai.theme.warning;
        default:         return root.color;
        }
    }

    font.italic: true
    color: palette.buttonText
    font.pointSize: isStateEvent? 0.95*Settings.uiFontSizePt : Settings.uiFontSizePt
    leftPadding: (isStateEvent && !stateEventIconOnRight) ? (stateEventIconSize + Komai.paddingSmall) : 0
    rightPadding: (isStateEvent && stateEventIconOnRight) ? (stateEventIconSize + Komai.paddingSmall) : 0

    FontMetrics {
        id: stateEventFontMetrics
        font: root.font
    }

    Image {
        id: stateEventIcon

        visible: root.isStateEvent
        anchors.left: root.stateEventIconOnRight ? undefined : root.left
        anchors.right: root.stateEventIconOnRight ? root.right : undefined
        y: Math.round((stateEventFontMetrics.lineSpacing - height) / 2)
        height: root.stateEventIconSize
        width: root.stateEventIconSize
        fillMode: Image.PreserveAspectFit
        source: root.stateEventIconSource ? ("image://colorimage/" + root.stateEventIconSource + "?" + root.stateEventIconColor) : ""
        sourceSize.height: root.stateEventIconSize
        sourceSize.width: root.stateEventIconSize
    }
}
