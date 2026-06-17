// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import cc.etke.komai

// Corner badge that marks a room avatar while a call is live in that room.
// Meant to be anchored to fill the avatar it decorates (tab bar, room list),
// drawn IN FRONT of it. Pair with RoomCallGlow (drawn behind) for the halo.
// Visual template: the room-header cog badge in AvatarSettingsFlipButton.qml
// (a light plate with a foreground-coloured glyph). Covers both call stacks
// (legacy GStreamer 1:1 and Element Call); only one call is active at a time.
Item {
    id: root

    property string roomId: ""
    // Glyph colour for a call you are on (green) vs a live call here you have
    // NOT joined (the warning hue). The badge sits on a light plate, so the raw
    // semantic theme colours read fine without the glow's brightness boost.
    property color accentColor: Komai.theme.success
    property color foreignColor: Komai.theme.warning
    // Badge fill: a light/neutral plate so the glyph stands out (like the cog
    // badge, which uses palette.window).
    property color badgeColor: palette.window
    property color glyphColor: foreignCall ? foreignColor : accentColor

    // You are on the call in this room (either call stack).
    readonly property bool joinedCall:
        roomId.length > 0
        && ((CallManager.isOnCall && CallManager.callRoomId === roomId)
            || (ElementCall.active && ElementCall.activeRoomId === roomId))
    // A call is live in this room but you are not on it. Rooms.activeCalls maps
    // roomId -> live participant count (undefined when no call); reactive via
    // activeCallsChanged.
    readonly property bool foreignCall:
        roomId.length > 0 && !joinedCall
        && Rooms.activeCalls[roomId] !== undefined
    readonly property bool callActive: joinedCall || foreignCall

    readonly property int badgeSize: Math.round(Math.min(width, height) * 0.46)
    readonly property int glyphSize: Math.round(badgeSize * 0.6)

    visible: callActive
    // Purely decorative: never steal pointer events from the tab/row beneath.
    enabled: false

    // Solid rounded badge with a call glyph, bottom-left so it clears the
    // pin badge (top-right) and the unread bubble (bottom-right).
    Rectangle {
        id: badge

        width: root.badgeSize
        height: root.badgeSize
        radius: Math.round(root.badgeSize * 0.28)
        color: root.badgeColor
        border.width: 1
        border.color: Qt.rgba(0, 0, 0, 0.18)

        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.leftMargin: -Math.round(root.badgeSize * 0.16)
        anchors.bottomMargin: -Math.round(root.badgeSize * 0.16)

        Image {
            anchors.centerIn: parent
            source: "image://colorimage/:/icons/icons/ui/place-call.svg?" + root.glyphColor
            sourceSize.width: root.glyphSize
            sourceSize.height: root.glyphSize
            width: root.glyphSize
            height: root.glyphSize
        }
    }
}
