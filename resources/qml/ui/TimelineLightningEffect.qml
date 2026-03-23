// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15

Item {
    id: root

    property int durationMs: 440
    property real flashOpacity: 0
    property real boltX: 0.5
    property real boltScale: 1.0
    property real boltRotation: 0
    property bool repeating: false
    property bool animationsEnabled: true

    anchors.fill: parent
    visible: flashOpacity > 0

    function pulseStrike()
    {
        boltX = 0.18 + Math.random() * 0.64;
        boltScale = 0.9 + Math.random() * 0.35;
        boltRotation = -18 + Math.random() * 36;
        flash.restart();
    }

    function scheduleNextRepeat()
    {
        repeatTimer.interval = 500 + Math.random() * 650;
        repeatTimer.restart();
    }

    function trigger(repeatStrikes)
    {
        if (!animationsEnabled) {
            triggerStatic();
            return;
        }
        repeatTimer.stop();
        repeating = !!repeatStrikes;
        pulseStrike();
        if (repeating)
            scheduleNextRepeat();
    }

    function triggerStatic()
    {
        flash.stop();
        repeatTimer.stop();
        repeating = false;
        boltX = 0.18 + Math.random() * 0.64;
        boltScale = 0.9 + Math.random() * 0.35;
        boltRotation = -18 + Math.random() * 36;
        flashOpacity = 0.95;
    }

    function reset()
    {
        flash.stop();
        repeatTimer.stop();
        repeating = false;
        flashOpacity = 0;
    }

    SequentialAnimation {
        id: flash

        running: false

        NumberAnimation {
            target: root
            property: "flashOpacity"
            to: 0.95
            duration: 45
        }
        PauseAnimation {
            duration: 35
        }
        NumberAnimation {
            target: root
            property: "flashOpacity"
            to: 0.3
            duration: 55
        }
        PauseAnimation {
            duration: 45
        }
        NumberAnimation {
            target: root
            property: "flashOpacity"
            to: 0.8
            duration: 40
        }
        NumberAnimation {
            target: root
            property: "flashOpacity"
            to: 0
            duration: 220
        }
    }

    Timer {
        id: repeatTimer

        repeat: false
        running: false

        onTriggered: {
            if (!root.repeating)
                return;

            root.pulseStrike();
            root.scheduleNextRepeat();
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#fff6cf"
        opacity: root.flashOpacity * 0.22
        visible: opacity > 0
    }

    Item {
        width: 120
        height: Math.min(root.height * 0.55, 240)
        x: root.width * root.boltX - width / 2
        y: 18
        visible: opacity > 0
        opacity: root.flashOpacity
        scale: root.boltScale
        rotation: root.boltRotation

        Rectangle {
            x: 18
            y: 18
            width: 66
            height: 66
            radius: 33
            color: "#fff8db"
            opacity: 0.18
        }

        Rectangle {
            x: 44
            y: 0
            width: 20
            height: 92
            radius: 10
            rotation: 18
            color: "#fffce9"
        }

        Rectangle {
            x: 56
            y: 76
            width: 20
            height: 88
            radius: 10
            rotation: -30
            color: "#ffd54f"
        }

        Rectangle {
            x: 28
            y: 146
            width: 18
            height: 78
            radius: 9
            rotation: 22
            color: "#fff176"
        }
    }
}
