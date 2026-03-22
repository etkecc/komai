// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15

Item {
    id: root

    property int durationMs: 1740
    property real overlayOpacity: 0
    property real haloOpacity: 0
    property real petalOpacity: 0
    property real ringOpacity: 0
    property real glintOpacity: 0
    property real logoOpacity: 0
    property real bloomScale: 0.58
    property real haloScale: 0.6
    property real ringScale: 0.62
    property real glintScale: 0.65
    property real logoScale: 0.8
    readonly property real activeOpacity: Math.max(overlayOpacity,
                                                   haloOpacity,
                                                   petalOpacity,
                                                   ringOpacity,
                                                   glintOpacity,
                                                   logoOpacity)

    anchors.fill: parent
    visible: activeOpacity > 0

    function trigger()
    {
        overlayOpacity = 0;
        haloOpacity = 0;
        petalOpacity = 0;
        ringOpacity = 0;
        glintOpacity = 0;
        logoOpacity = 0;
        bloomScale = 0.58;
        haloScale = 0.6;
        ringScale = 0.62;
        glintScale = 0.65;
        logoScale = 0.8;
        burst.restart();
    }

    function reset()
    {
        burst.stop();
        overlayOpacity = 0;
        haloOpacity = 0;
        petalOpacity = 0;
        ringOpacity = 0;
        glintOpacity = 0;
        logoOpacity = 0;
    }

    SequentialAnimation {
        id: burst

        running: false

        ParallelAnimation {
            NumberAnimation {
                target: root
                property: "overlayOpacity"
                to: 1
                duration: 140
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: root
                property: "haloOpacity"
                to: 1
                duration: 180
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: root
                property: "petalOpacity"
                to: 1
                duration: 210
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: root
                property: "ringOpacity"
                to: 0.8
                duration: 200
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: root
                property: "glintOpacity"
                to: 1
                duration: 180
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: root
                property: "logoOpacity"
                to: 1
                duration: 200
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: root
                property: "bloomScale"
                to: 1
                duration: 260
                easing.type: Easing.OutBack
            }
            NumberAnimation {
                target: root
                property: "haloScale"
                to: 1
                duration: 240
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: root
                property: "ringScale"
                to: 1.08
                duration: 340
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: root
                property: "glintScale"
                to: 1
                duration: 220
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: root
                property: "logoScale"
                to: 1
                duration: 240
                easing.type: Easing.OutBack
            }
        }
        PauseAnimation {
            duration: 280
        }
        ParallelAnimation {
            NumberAnimation {
                target: root
                property: "overlayOpacity"
                to: 0
                duration: 980
                easing.type: Easing.InOutQuad
            }
            NumberAnimation {
                target: root
                property: "haloOpacity"
                to: 0
                duration: 1120
                easing.type: Easing.InOutQuad
            }
            NumberAnimation {
                target: root
                property: "petalOpacity"
                to: 0
                duration: 1080
                easing.type: Easing.InOutQuad
            }
            NumberAnimation {
                target: root
                property: "ringOpacity"
                to: 0
                duration: 820
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: root
                property: "glintOpacity"
                to: 0
                duration: 720
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: root
                property: "logoOpacity"
                to: 0
                duration: 920
                easing.type: Easing.InOutQuad
            }
            NumberAnimation {
                target: root
                property: "haloScale"
                to: 1.08
                duration: 1120
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: root
                property: "ringScale"
                to: 1.22
                duration: 820
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: root
                property: "glintScale"
                to: 1.06
                duration: 720
                easing.type: Easing.OutCubic
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#ffd7e6"
        opacity: root.overlayOpacity * 0.09
        visible: opacity > 0
    }

    Item {
        width: Math.min(Math.max(root.width * 0.39, 250), 360)
        height: width
        x: root.width * 0.5 - width / 2
        y: Math.min(Math.max(root.height * 0.14, 34), root.height * 0.24)
        visible: opacity > 0
        opacity: Math.max(root.haloOpacity,
                          root.petalOpacity,
                          root.ringOpacity,
                          root.glintOpacity,
                          root.logoOpacity)

        Rectangle {
            anchors.centerIn: parent
            width: parent.width * 0.96
            height: width
            radius: width / 2
            color: "#ffe4ed"
            opacity: root.haloOpacity * 0.38
            scale: root.haloScale
        }

        Repeater {
            model: 6

            Item {
                anchors.fill: parent
                rotation: index * 60 + 30
                transformOrigin: Item.Center

                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: parent.height * 0.03
                    width: Math.max(parent.width * 0.026, 10)
                    height: parent.height * 0.27
                    radius: width / 2
                    color: "#ffe9a3"
                    opacity: root.glintOpacity * 0.34
                    scale: root.glintScale
                    transformOrigin: Item.Bottom
                }
            }
        }

        Repeater {
            model: 5

            Item {
                anchors.fill: parent
                rotation: index * 72
                transformOrigin: Item.Center

                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: parent.height * 0.08
                    width: parent.width * 0.22
                    height: parent.height * 0.39
                    radius: width / 2
                    color: "#ffb9cf"
                    opacity: root.petalOpacity * 0.88
                    scale: root.bloomScale
                    transformOrigin: Item.Bottom
                }

                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: parent.height * 0.12
                    width: parent.width * 0.12
                    height: parent.height * 0.23
                    radius: width / 2
                    color: "#ffd7e3"
                    opacity: root.petalOpacity * 0.78
                    scale: root.bloomScale
                    transformOrigin: Item.Bottom
                }
            }
        }

        Rectangle {
            anchors.centerIn: parent
            width: parent.width * 0.18
            height: width
            radius: width / 2
            color: "#fff1c8"
            opacity: root.petalOpacity * 0.94
            scale: root.bloomScale
        }

        Rectangle {
            anchors.centerIn: parent
            width: parent.width * 0.74
            height: width
            radius: width / 2
            color: "transparent"
            border.width: Math.max(2, Math.round(parent.width * 0.012))
            border.color: "#ffd4a3"
            opacity: root.ringOpacity * 0.7
            scale: root.ringScale
        }

        Repeater {
            model: [
                { "xRatio": 0.16, "yRatio": 0.28, "rotation": -18, "size": 0.11 },
                { "xRatio": 0.82, "yRatio": 0.2, "rotation": 20, "size": 0.1 },
                { "xRatio": 0.76, "yRatio": 0.82, "rotation": -8, "size": 0.09 }
            ]

            Item {
                width: parent.width * modelData.size
                height: width
                x: parent.width * modelData.xRatio - width / 2
                y: parent.height * modelData.yRatio - height / 2
                rotation: modelData.rotation
                opacity: root.glintOpacity
                scale: root.glintScale

                Rectangle {
                    anchors.centerIn: parent
                    width: parent.width * 0.22
                    height: parent.height
                    radius: width / 2
                    color: "#fff6db"
                }

                Rectangle {
                    anchors.centerIn: parent
                    width: parent.width
                    height: parent.height * 0.22
                    radius: height / 2
                    color: "#fff6db"
                }
            }
        }

        Rectangle {
            anchors.centerIn: parent
            width: parent.width * 0.28
            height: width
            radius: width / 2
            color: "#fff7ea"
            opacity: root.logoOpacity * 0.88
            scale: root.logoScale
        }

        Image {
            anchors.centerIn: parent
            width: parent.width * 0.18
            height: width
            source: "qrc:/logos/komai.svg"
            fillMode: Image.PreserveAspectFit
            smooth: true
            mipmap: true
            opacity: root.logoOpacity
            scale: root.logoScale
        }
    }
}
