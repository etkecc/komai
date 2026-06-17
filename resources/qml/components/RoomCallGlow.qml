// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Effects
import cc.etke.komai

// Diffused glow halo for a room avatar while a call is live in that room.
// Place this as a direct sibling of the avatar image with a z BELOW it
// (e.g. z: -1) and anchored to fill it: the source is a filled avatar-shaped
// disk that MultiEffect blurs into a soft bloom; the opaque avatar in front
// masks the disk centre, leaving only the halo around the edge. Covers both
// call stacks (legacy GStreamer 1:1 and Element Call); one call at a time.
Item {
    id: root

    property string roomId: ""
    // Avatar shape it sits behind, so the disk matches the avatar corners.
    property bool circular: Settings.uiAvatarsCircular

    // The glow is a diffuse, breathing bloom painted OVER the row/tab highlight
    // background, which varies per theme and per selection state. The theme's
    // semantic colours are tuned for TEXT legibility (e.g. success is #008000,
    // warning #9c5e00 on light themes) and wash out as a bloom. So we follow the
    // theme's HUE but normalize it to a bright, saturated value via glowify(),
    // keeping consistent salience on white, on mid-grey highlights, and on dark
    // themes alike.
    function glowify(c) {
        return Qt.hsla(c.hslHue < 0 ? 0 : c.hslHue,
                       Math.max(c.hslSaturation, 0.6), 0.55, 1.0)
    }
    // A call you are on (green, same family as the legacy ActiveCallBar).
    property color accentColor: glowify(Komai.theme.success)
    // A live call in this room you have NOT joined ("you could join" — the
    // warning hue: attention, without the danger meaning of error/red).
    property color foreignColor: glowify(Komai.theme.warning)

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
    readonly property color effectiveColor: foreignCall ? foreignColor : accentColor

    // How far the faint outer bloom reaches past the avatar edge (drives the
    // blur radius below).
    readonly property int glowSpread: Math.round(Math.min(width, height) * 0.55)
    // How far the SOLID disk pokes past the avatar edge before it blurs. This
    // dense band hugging the avatar is the most VISIBLE part of the glow, so it
    // dominates how far the glow appears to reach into neighbouring rows/tabs.
    readonly property int rimBeyond: Math.round(Math.min(width, height) * 0.08)

    visible: callActive
    enabled: false

    Item {
        id: glowSource

        anchors.fill: parent
        anchors.margins: -root.glowSpread
        visible: false
        layer.enabled: true

        // Filled disk a little larger than the avatar's footprint, so a dense
        // band of colour shows around the avatar edge; after the blur its outer
        // edge blooms into the surrounding margin. The avatar (drawn in front)
        // hides the solid centre.
        Rectangle {
            anchors.fill: parent
            anchors.margins: root.glowSpread - root.rimBeyond
            radius: root.circular ? width / 2 : Math.round(width / 8)
            color: root.effectiveColor
        }
    }

    MultiEffect {
        id: glowEffect

        anchors.fill: glowSource
        source: glowSource
        blurEnabled: true
        blur: 1.0
        // Proportional to the avatar size: a fixed pixel radius over-blurs the
        // small tab/list avatars and washes the colour out to nothing.
        blurMax: Math.max(8, Math.round(root.glowSpread * 1.2))
        transformOrigin: Item.Center

        // Pronounced "breathing" (scale + opacity) so the halo is unmistakably
        // alive even where its colour contrast with the backdrop is modest;
        // motion is detectable regardless of contrast. Honours the
        // reduce-motion setting (holds steady, fully shown, when motion is off).
        SequentialAnimation on scale {
            running: root.visible && Settings.uiMotionAnimationsEnabled
            loops: Animation.Infinite
            NumberAnimation { from: 0.96; to: 1.09; duration: 1000; easing.type: Easing.InOutSine }
            NumberAnimation { from: 1.09; to: 0.96; duration: 1000; easing.type: Easing.InOutSine }
        }
        SequentialAnimation on opacity {
            running: root.visible && Settings.uiMotionAnimationsEnabled
            loops: Animation.Infinite
            NumberAnimation { from: 1.0; to: 0.35; duration: 1000; easing.type: Easing.InOutSine }
            NumberAnimation { from: 0.35; to: 1.0; duration: 1000; easing.type: Easing.InOutSine }
        }
    }
}
