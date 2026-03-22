// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15

Item {
    id: root

    property int durationMs: 1700
    property real overlayOpacity: 0
    property real haloOpacity: 0
    property real ringOpacity: 0
    property real heartOpacity: 0
    property real sparkleOpacity: 0
    property real haloScale: 0.7
    property real ringScale: 0.72
    property real heartScale: 0.68
    property real sparkleScale: 0.7
    property real heartLift: 18
    readonly property real activeOpacity: Math.max(overlayOpacity,
                                                   haloOpacity,
                                                   ringOpacity,
                                                   heartOpacity,
                                                   sparkleOpacity)

    anchors.fill: parent
    visible: activeOpacity > 0

    function trigger()
    {
        overlayOpacity = 0;
        haloOpacity = 0;
        ringOpacity = 0;
        heartOpacity = 0;
        sparkleOpacity = 0;
        haloScale = 0.7;
        ringScale = 0.72;
        heartScale = 0.68;
        sparkleScale = 0.7;
        heartLift = 18;
        burst.restart();
    }

    function reset()
    {
        burst.stop();
        overlayOpacity = 0;
        haloOpacity = 0;
        ringOpacity = 0;
        heartOpacity = 0;
        sparkleOpacity = 0;
    }

    component HeartGlyph: Item {
        id: heart

        property color fillColor: "#ff6fa5"
        property real glowOpacity: 0.2

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            y: parent.height * 0.3
            width: parent.width * 0.56
            height: parent.height * 0.56
            radius: parent.width * 0.08
            rotation: 45
            color: heart.fillColor
        }

        Rectangle {
            x: parent.width * 0.1
            y: parent.height * 0.06
            width: parent.width * 0.46
            height: width
            radius: width / 2
            color: heart.fillColor
        }

        Rectangle {
            x: parent.width * 0.44
            y: parent.height * 0.06
            width: parent.width * 0.46
            height: width
            radius: width / 2
            color: heart.fillColor
        }

        Rectangle {
            anchors.centerIn: parent
            width: parent.width * 0.9
            height: width
            radius: width / 2
            color: "#ffd6e6"
            opacity: heart.glowOpacity
            scale: 1.12
        }
    }

    SequentialAnimation {
        id: burst

        running: false

        ParallelAnimation {
            NumberAnimation {
                target: root
                property: "overlayOpacity"
                to: 1
                duration: 160
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: root
                property: "haloOpacity"
                to: 1
                duration: 220
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: root
                property: "ringOpacity"
                to: 0.74
                duration: 240
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: root
                property: "heartOpacity"
                to: 1
                duration: 260
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: root
                property: "sparkleOpacity"
                to: 1
                duration: 180
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: root
                property: "haloScale"
                to: 1
                duration: 340
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: root
                property: "ringScale"
                to: 1.06
                duration: 300
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: root
                property: "heartScale"
                to: 1
                duration: 320
                easing.type: Easing.OutBack
            }
            NumberAnimation {
                target: root
                property: "sparkleScale"
                to: 1
                duration: 240
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: root
                property: "heartLift"
                to: 0
                duration: 320
                easing.type: Easing.OutCubic
            }
        }
        PauseAnimation {
            duration: 220
        }
        ParallelAnimation {
            NumberAnimation {
                target: root
                property: "overlayOpacity"
                to: 0
                duration: 900
                easing.type: Easing.InOutQuad
            }
            NumberAnimation {
                target: root
                property: "haloOpacity"
                to: 0
                duration: 980
                easing.type: Easing.InOutQuad
            }
            NumberAnimation {
                target: root
                property: "ringOpacity"
                to: 0
                duration: 760
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: root
                property: "heartOpacity"
                to: 0
                duration: 940
                easing.type: Easing.InOutQuad
            }
            NumberAnimation {
                target: root
                property: "sparkleOpacity"
                to: 0
                duration: 700
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: root
                property: "haloScale"
                to: 1.1
                duration: 980
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: root
                property: "ringScale"
                to: 1.24
                duration: 760
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: root
                property: "heartScale"
                to: 1.06
                duration: 940
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: root
                property: "sparkleScale"
                to: 1.08
                duration: 700
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: root
                property: "heartLift"
                to: -24
                duration: 940
                easing.type: Easing.OutCubic
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#ffc1d9"
        opacity: root.overlayOpacity * 0.08
        visible: opacity > 0
    }

    Item {
        width: Math.min(Math.max(root.width * 0.38, 240), 360)
        height: Math.round(width * 0.88)
        x: root.width * 0.5 - width / 2
        y: Math.min(Math.max(root.height * 0.2, 44), root.height * 0.3)
        visible: opacity > 0
        opacity: Math.max(root.haloOpacity, root.ringOpacity, root.heartOpacity, root.sparkleOpacity)

        Rectangle {
            anchors.centerIn: parent
            width: parent.width * 0.9
            height: width
            radius: width / 2
            color: "#ffd7e6"
            opacity: root.haloOpacity * 0.32
            scale: root.haloScale
        }

        Rectangle {
            anchors.centerIn: parent
            width: parent.width * 0.66
            height: width
            radius: width / 2
            color: "transparent"
            border.width: Math.max(2, Math.round(parent.width * 0.012))
            border.color: "#ffb2c9"
            opacity: root.ringOpacity * 0.76
            scale: root.ringScale
        }

        Repeater {
            model: [
                { "xRatio": 0.28, "yRatio": 0.52, "size": 0.23, "rotation": -14, "rise": 0.75, "color": "#ff77ab" },
                { "xRatio": 0.5, "yRatio": 0.34, "size": 0.3, "rotation": 0, "rise": 1.0, "color": "#ff5f97" },
                { "xRatio": 0.72, "yRatio": 0.54, "size": 0.2, "rotation": 14, "rise": 0.7, "color": "#ff86b4" }
            ]

            Item {
                width: parent.width * modelData.size
                height: width
                x: parent.width * modelData.xRatio - width / 2
                y: parent.height * modelData.yRatio - height / 2 + root.heartLift * modelData.rise
                rotation: modelData.rotation
                opacity: root.heartOpacity
                scale: root.heartScale

                HeartGlyph {
                    anchors.fill: parent
                    fillColor: modelData.color
                    glowOpacity: 0.18
                }
            }
        }

        Repeater {
            model: [
                { "xRatio": 0.16, "yRatio": 0.3, "rotation": -14, "size": 0.12 },
                { "xRatio": 0.84, "yRatio": 0.26, "rotation": 16, "size": 0.11 },
                { "xRatio": 0.22, "yRatio": 0.78, "rotation": -6, "size": 0.1 },
                { "xRatio": 0.79, "yRatio": 0.74, "rotation": 8, "size": 0.09 }
            ]

            Item {
                width: parent.width * modelData.size
                height: width
                x: parent.width * modelData.xRatio - width / 2
                y: parent.height * modelData.yRatio - height / 2
                rotation: modelData.rotation
                opacity: root.sparkleOpacity
                scale: root.sparkleScale

                Rectangle {
                    anchors.centerIn: parent
                    width: parent.width * 0.22
                    height: parent.height
                    radius: width / 2
                    color: "#fff7df"
                }

                Rectangle {
                    anchors.centerIn: parent
                    width: parent.width
                    height: parent.height * 0.22
                    radius: height / 2
                    color: "#fff7df"
                }
            }
        }
    }
}
