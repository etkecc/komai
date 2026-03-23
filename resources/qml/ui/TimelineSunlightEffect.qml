// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15

Item {
    id: root

    property int durationMs: 2200
    property real overlayOpacity: 0
    property real haloOpacity: 0
    property real discOpacity: 0
    property real rayOpacity: 0
    property real discScale: 0.7
    property real haloScale: 0.72
    property real rayScale: 0.68
    property real centerX: 0.82
    property real centerY: 0.14
    property bool animationsEnabled: true
    readonly property real activeOpacity: Math.max(overlayOpacity, haloOpacity, discOpacity, rayOpacity)

    anchors.fill: parent
    visible: activeOpacity > 0

    function trigger()
    {
        centerX = 0.78 + Math.random() * 0.1;
        centerY = 0.08 + Math.random() * 0.08;
        if (!animationsEnabled) {
            triggerStatic();
            return;
        }
        overlayOpacity = 0;
        haloOpacity = 0;
        discOpacity = 0;
        rayOpacity = 0;
        discScale = 0.7;
        haloScale = 0.72;
        rayScale = 0.68;
        burst.restart();
    }

    function triggerStatic()
    {
        burst.stop();
        overlayOpacity = 1;
        haloOpacity = 1;
        discOpacity = 1;
        rayOpacity = 0.72;
        discScale = 1;
        haloScale = 1.06;
        rayScale = 1;
    }

    function reset()
    {
        burst.stop();
        overlayOpacity = 0;
        haloOpacity = 0;
        discOpacity = 0;
        rayOpacity = 0;
    }

    SequentialAnimation {
        id: burst

        running: false

        ParallelAnimation {
            NumberAnimation {
                target: root
                property: "overlayOpacity"
                to: 1
                duration: 220
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: root
                property: "haloOpacity"
                to: 1
                duration: 260
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: root
                property: "discOpacity"
                to: 1
                duration: 230
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: root
                property: "rayOpacity"
                to: 0.72
                duration: 240
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: root
                property: "discScale"
                to: 1
                duration: 320
                easing.type: Easing.OutBack
            }
            NumberAnimation {
                target: root
                property: "haloScale"
                to: 1.06
                duration: 340
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: root
                property: "rayScale"
                to: 1
                duration: 300
                easing.type: Easing.OutCubic
            }
        }
        PauseAnimation {
            duration: 260
        }
        ParallelAnimation {
            NumberAnimation {
                target: root
                property: "overlayOpacity"
                to: 0
                duration: 1400
                easing.type: Easing.InOutQuad
            }
            NumberAnimation {
                target: root
                property: "haloOpacity"
                to: 0
                duration: 1450
                easing.type: Easing.InOutQuad
            }
            NumberAnimation {
                target: root
                property: "discOpacity"
                to: 0
                duration: 1250
                easing.type: Easing.InOutQuad
            }
            NumberAnimation {
                target: root
                property: "rayOpacity"
                to: 0
                duration: 1180
                easing.type: Easing.InOutQuad
            }
            NumberAnimation {
                target: root
                property: "haloScale"
                to: 1.18
                duration: 1450
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: root
                property: "rayScale"
                to: 1.08
                duration: 1180
                easing.type: Easing.OutCubic
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#ffde7a"
        opacity: root.overlayOpacity * 0.11
        visible: opacity > 0
    }

    Item {
        width: Math.min(Math.max(root.width * 0.55, 320), 560)
        height: width
        x: root.width * root.centerX - width / 2
        y: root.height * root.centerY - height / 2
        visible: opacity > 0
        opacity: Math.max(root.haloOpacity, root.discOpacity, root.rayOpacity)

        Rectangle {
            anchors.centerIn: parent
            width: parent.width * 0.94
            height: width
            radius: width / 2
            color: "#ffe082"
            opacity: root.haloOpacity * 0.24
            scale: root.haloScale
        }

        Repeater {
            model: 8

            Rectangle {
                width: Math.max(parent.width * 0.03, 16)
                height: parent.width * 0.44
                radius: width / 2
                anchors.centerIn: parent
                color: "#ffd54f"
                opacity: root.rayOpacity * (index % 2 === 0 ? 0.36 : 0.26)
                rotation: index * 45
                scale: root.rayScale
                transformOrigin: Item.Center
            }
        }

        Rectangle {
            anchors.centerIn: parent
            width: Math.min(parent.width * 0.31, 150)
            height: width
            radius: width / 2
            color: "#fff1a8"
            opacity: root.discOpacity * 0.95
            scale: root.discScale
        }

        Rectangle {
            anchors.centerIn: parent
            width: Math.min(parent.width * 0.18, 90)
            height: width
            radius: width / 2
            color: "#fff9de"
            opacity: root.discOpacity
            scale: root.discScale * 0.94
        }
    }
}
