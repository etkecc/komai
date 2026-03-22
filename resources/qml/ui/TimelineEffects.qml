// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15
import QtQuick.Particles 2.15

Item {
    id: effectRoot
    readonly property var effectEmitters: ({
        "confetti": confettiEmitter,
        "rainfall": rainfallEmitter,
        "komaiLogo": komaiEmitter
    })
    readonly property var effectPulseScales: ({
        "confetti": 2.0,
        "rainfall": 3.3,
        "komaiLogo": 3.3
    })
    readonly property var effectDurations: ({
        "confetti": confettiEmitter.lifeSpan,
        "rainfall": rainfallEmitter.lifeSpan,
        "lightning": 440,
        "komaiLogo": komaiEmitter.lifeSpan
    })
    readonly property int maxEffectDuration: {
        var max = 0;
        for (var name in effectDurations) {
            var duration = effectDurations[name];
            if (duration > max)
                max = duration;
        }
        return max;
    }
    required property bool shouldEffectsRun
    property real lightningFlashOpacity: 0
    property real lightningBoltX: 0.5
    property real lightningBoltScale: 1.0
    property real lightningBoltRotation: 0
    visible: effectRoot.shouldEffectsRun

    function durationForEffects(effectNames)
    {
        if (!effectNames || effectNames.length === 0)
            return maxEffectDuration;

        let max = 0;
        for (let i = 0; i < effectNames.length; ++i) {
            const duration = effectDurations[effectNames[i]] || 0;
            if (duration > max)
                max = duration;
        }

        return max || maxEffectDuration;
    }

    function pulseEffects(effectNames)
    {
        if (!effectNames)
            return;

        for (let i = 0; i < effectNames.length; ++i)
            pulseEffect(effectNames[i]);
    }

    function pulseEffect(effectName)
    {
        if (effectName === "lightning") {
            pulseLightning();
            return;
        }

        const emitter = effectEmitters[effectName];
        if (!emitter) {
            console.warn("Unknown timeline effect:", effectName);
            return;
        }

        const scale = effectPulseScales[effectName] || 1.0;
        emitter.pulse(effectRoot.height * scale);
    }

    function pulseLightning()
    {
        lightningBoltX = 0.18 + Math.random() * 0.64;
        lightningBoltScale = 0.9 + Math.random() * 0.35;
        lightningBoltRotation = -18 + Math.random() * 36;
        lightningFlash.restart();
    }

    function removeParticles()
    {
        particleSystem.reset()
        lightningFlash.stop()
        lightningFlashOpacity = 0
    }

    ParticleSystem {
        id: particleSystem

        Component.onCompleted: stop();
        paused: !effectRoot.shouldEffectsRun
        running: effectRoot.shouldEffectsRun
    }

    SequentialAnimation {
        id: lightningFlash

        running: false

        NumberAnimation {
            target: effectRoot
            property: "lightningFlashOpacity"
            to: 0.95
            duration: 45
        }
        PauseAnimation {
            duration: 35
        }
        NumberAnimation {
            target: effectRoot
            property: "lightningFlashOpacity"
            to: 0.3
            duration: 55
        }
        PauseAnimation {
            duration: 45
        }
        NumberAnimation {
            target: effectRoot
            property: "lightningFlashOpacity"
            to: 0.8
            duration: 40
        }
        NumberAnimation {
            target: effectRoot
            property: "lightningFlashOpacity"
            to: 0
            duration: 220
        }
    }

    Rectangle {
        anchors.fill: parent
        z: 10
        color: "#fff6cf"
        opacity: effectRoot.lightningFlashOpacity * 0.22
        visible: opacity > 0
    }

    Item {
        width: 120
        height: Math.min(effectRoot.height * 0.55, 240)
        x: effectRoot.width * effectRoot.lightningBoltX - width / 2
        y: 18
        z: 11
        visible: opacity > 0
        opacity: effectRoot.lightningFlashOpacity
        scale: effectRoot.lightningBoltScale
        rotation: effectRoot.lightningBoltRotation

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

    Emitter {
        id: confettiEmitter

        group: "confetti"
        width: effectRoot.width * 3/4
        enabled: false
        anchors.horizontalCenter: effectRoot.horizontalCenter
        y: effectRoot.height
        emitRate: Math.min(400 * Math.sqrt(effectRoot.width * effectRoot.height) / 870, 1000)
        lifeSpan: 15000
        system: particleSystem
        maximumEmitted: 500
        velocityFromMovement: 8
        size: 16
        sizeVariation: 4
        velocity: PointDirection {
            x: 0
            y: -Math.min(450 * effectRoot.height / 700, 1000)
            xVariation: Math.min(4 * effectRoot.width / 7, 450)
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
        anchors.fill: effectRoot
        magnitude: 350
        angle: 90
    }

    Emitter {
        id: rainfallEmitter

        group: "rain"
        width: effectRoot.width
        enabled: false
        anchors.horizontalCenter: effectRoot.horizontalCenter
        y: -60
        emitRate: effectRoot.width / 30
        lifeSpan: 10000
        system: particleSystem
        velocity: PointDirection {
            x: 0
            y: 400
            xVariation: 0
            yVariation: 75
        }

        // causes high CPU load, see: https://bugreports.qt.io/browse/QTBUG-117923
        //ItemParticle {
            //    system: particleSystem
            //    groups: ["rain"]
            //    fade: false
            //    visible: effectRoot.shouldEffectsRun
            //    delegate: Rectangle {
            //        width: 2
            //        height: 30 + 30 * Math.random()
            //        radius: 2
            //        color: "#0099ff"
            //    }
            //}

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
        }

    Emitter {
        id: komaiEmitter

        group: "komai"
        width: effectRoot.width
        enabled: false
        anchors.horizontalCenter: effectRoot.horizontalCenter
        y: -60
        emitRate: effectRoot.width / 100
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
        anchors.fill: effectRoot
        strength: 300
    }
}
