// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15
import QtQuick.Particles 2.15

Item {
    id: root

    required property bool shouldEffectsRun

    property alias particleSystem: particleSystem
    property alias confettiEmitter: confettiEmitter
    property alias rainfallEmitter: rainfallEmitter
    property alias komaiEmitter: komaiEmitter

    anchors.fill: parent

    ParticleSystem {
        id: particleSystem

        Component.onCompleted: stop();
        paused: !root.shouldEffectsRun
        running: root.shouldEffectsRun
    }

    Emitter {
        id: confettiEmitter

        group: "confetti"
        width: root.width * 3/4
        enabled: false
        anchors.horizontalCenter: root.horizontalCenter
        y: root.height
        emitRate: Math.min(400 * Math.sqrt(root.width * root.height) / 870, 1000)
        lifeSpan: 15000
        system: particleSystem
        maximumEmitted: 500
        velocityFromMovement: 8
        size: 16
        sizeVariation: 4
        velocity: PointDirection {
            x: 0
            y: -Math.min(450 * root.height / 700, 1000)
            xVariation: Math.min(4 * root.width / 7, 450)
            yVariation: 250
        }
    }

    ImageParticle {
        system: particleSystem
        groups: ["confetti"]
        source: "qrc:/confettiparticle.svg"
        rotationVelocity: 0
        rotationVelocityVariation: 360
        colorVariation: 1
        color: "white"
        entryEffect: ImageParticle.None
        xVector: PointDirection {
            x: 1
            y: 0
            xVariation: 0.2
            yVariation: 0.2
        }
        yVector: PointDirection {
            x: 0
            y: 0.5
            xVariation: 0.2
            yVariation: 0.2
        }
    }

    Gravity {
        system: particleSystem
        groups: ["confetti"]
        anchors.fill: root
        magnitude: 350
        angle: 90
    }

    Emitter {
        id: rainfallEmitter

        group: "rain"
        width: root.width * 1.15
        enabled: false
        anchors.horizontalCenter: root.horizontalCenter
        y: -60
        emitRate: Math.min(root.width / 1.5, 950)
        lifeSpan: 1800
        system: particleSystem
        velocity: PointDirection {
            x: 0
            y: 700
            xVariation: 0
            yVariation: 180
        }
    }

    ImageParticle {
        system: particleSystem
        groups: ["rain"]
        source: "qrc:/confettiparticle.svg"
        rotationVelocity: 0
        rotationVelocityVariation: 0
        colorVariation: 0
        color: "#0099ff"
        entryEffect: ImageParticle.None
        xVector: PointDirection {
            x: 0.01
            y: 0
        }
        yVector: PointDirection {
            x: 0
            y: 5
        }
    }

    Emitter {
        id: komaiEmitter

        group: "komai"
        width: root.width
        enabled: false
        anchors.horizontalCenter: root.horizontalCenter
        y: -60
        emitRate: root.width / 100
        lifeSpan: 7000
        system: particleSystem
        size: 28
        sizeVariation: 3
        velocity: PointDirection {
            x: 0
            y: 350
            xVariation: 100
            yVariation: 60
        }
    }

    ImageParticle {
        system: particleSystem
        groups: ["komai"]
        source: "qrc:/logos/komai.svg"
        rotationVelocity: 0
        rotationVelocityVariation: 90
        colorVariation: 0
        color: "white"
        entryEffect: ImageParticle.None
        xVector: PointDirection {
            x: 1
            y: 0
            xVariation: 0.2
            yVariation: 0.2
        }
        yVector: PointDirection {
            x: 0
            y: 1
            xVariation: 0.2
            yVariation: 0.2
        }
    }

    Turbulence {
        system: particleSystem
        groups: ["komai"]
        anchors.fill: root
        strength: 300
    }
}
