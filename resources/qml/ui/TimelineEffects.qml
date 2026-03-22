// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15
import QtQuick.Particles 2.15

Item {
    id: effectRoot
    readonly property var effectEmitters: ({
        "confetti": particlesLoader.item ? particlesLoader.item.confettiEmitter : null,
        "rainfall": particlesLoader.item ? particlesLoader.item.rainfallEmitter : null,
        "komaiLogo": particlesLoader.item ? particlesLoader.item.komaiEmitter : null
    })
    readonly property var effectPulseScales: ({
        "confetti": 2.0,
        "rainfall": 3.3,
        "komaiLogo": 3.3
    })
    readonly property int maxEffectDuration: {
        var max = 0;
        const effectNames = ["confetti", "sunlight", "rainfall", "lightning", "komaiLogo"];
        for (let i = 0; i < effectNames.length; ++i) {
            var duration = effectDuration(effectNames[i]);
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
    property bool lightningRepeaterActive: false
    property real sunlightOverlayOpacity: 0
    property real sunlightHaloOpacity: 0
    property real sunlightDiscOpacity: 0
    property real sunlightRayOpacity: 0
    property real sunlightDiscScale: 0.7
    property real sunlightHaloScale: 0.72
    property real sunlightRayScale: 0.68
    property real sunlightCenterX: 0.82
    property real sunlightCenterY: 0.14
    visible: effectRoot.shouldEffectsRun

    function durationForEffects(effectNames)
    {
        if (!effectNames || effectNames.length === 0)
            return maxEffectDuration;

        let max = 0;
        for (let i = 0; i < effectNames.length; ++i) {
            const duration = effectDuration(effectNames[i]);
            if (duration > max)
                max = duration;
        }

        return max || maxEffectDuration;
    }

    function pulseDurationForEffect(effectName)
    {
        if (effectName === "lightning" || effectName === "sunlight")
            return 0;

        const scale = effectPulseScales[effectName] || 1.0;
        let pulseDuration = effectRoot.height * scale;
        if (effectName === "rainfall")
            pulseDuration = Math.min(pulseDuration, 1200);
        return pulseDuration;
    }

    function effectDuration(effectName)
    {
        if (effectName === "lightning")
            return 440;
        if (effectName === "sunlight")
            return 2200;

        const emitter = effectEmitters[effectName];
        if (!emitter)
            return 0;

        return pulseDurationForEffect(effectName) + emitter.lifeSpan;
    }

    function pulseEffects(effectNames)
    {
        if (!effectNames)
            return;

        ensureParticleLayer();

        for (let i = 0; i < effectNames.length; ++i)
            pulseEffect(effectNames[i]);

        if (effectNames.indexOf("rainfall") !== -1 && effectNames.indexOf("lightning") !== -1)
            startLightningRepeater();
    }

    function pulseEffect(effectName)
    {
        if (effectName === "lightning") {
            pulseLightning();
            return;
        }
        if (effectName === "sunlight") {
            pulseSunlight();
            return;
        }

        const emitter = effectEmitters[effectName];
        if (!emitter) {
            console.warn("Unknown timeline effect:", effectName);
            return;
        }

        emitter.pulse(pulseDurationForEffect(effectName));
    }

    function ensureParticleLayer()
    {
        if (!particlesLoader.active)
            particlesLoader.active = true;
    }

    function pulseLightning()
    {
        lightningBoltX = 0.18 + Math.random() * 0.64;
        lightningBoltScale = 0.9 + Math.random() * 0.35;
        lightningBoltRotation = -18 + Math.random() * 36;
        lightningFlash.restart();
    }

    function pulseSunlight()
    {
        sunlightCenterX = 0.78 + Math.random() * 0.1;
        sunlightCenterY = 0.08 + Math.random() * 0.08;
        sunlightOverlayOpacity = 0;
        sunlightHaloOpacity = 0;
        sunlightDiscOpacity = 0;
        sunlightRayOpacity = 0;
        sunlightDiscScale = 0.7;
        sunlightHaloScale = 0.72;
        sunlightRayScale = 0.68;
        sunlightBurst.restart();
    }

    function scheduleNextLightningRepeat()
    {
        lightningRepeatTimer.interval = 500 + Math.random() * 650;
        lightningRepeatTimer.restart();
    }

    function startLightningRepeater()
    {
        lightningRepeaterActive = true;
        scheduleNextLightningRepeat();
    }

    function removeParticles()
    {
        sunlightBurst.stop()
        lightningFlash.stop()
        lightningRepeatTimer.stop()
        lightningRepeaterActive = false
        sunlightOverlayOpacity = 0
        sunlightHaloOpacity = 0
        sunlightDiscOpacity = 0
        sunlightRayOpacity = 0
        lightningFlashOpacity = 0
        particlesLoader.active = false
        Qt.callLater(function() {
            particlesLoader.active = true;
        })
    }

    Component {
        id: particleLayerComponent

        TimelineParticleLayer {
            shouldEffectsRun: effectRoot.shouldEffectsRun
        }
    }

    Loader {
        id: particlesLoader

        anchors.fill: parent
        active: true
        sourceComponent: particleLayerComponent
    }

    SequentialAnimation {
        id: sunlightBurst

        running: false

        ParallelAnimation {
            NumberAnimation {
                target: effectRoot
                property: "sunlightOverlayOpacity"
                to: 1
                duration: 220
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: effectRoot
                property: "sunlightHaloOpacity"
                to: 1
                duration: 260
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: effectRoot
                property: "sunlightDiscOpacity"
                to: 1
                duration: 230
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: effectRoot
                property: "sunlightRayOpacity"
                to: 0.72
                duration: 240
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: effectRoot
                property: "sunlightDiscScale"
                to: 1
                duration: 320
                easing.type: Easing.OutBack
            }
            NumberAnimation {
                target: effectRoot
                property: "sunlightHaloScale"
                to: 1.06
                duration: 340
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: effectRoot
                property: "sunlightRayScale"
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
                target: effectRoot
                property: "sunlightOverlayOpacity"
                to: 0
                duration: 1400
                easing.type: Easing.InOutQuad
            }
            NumberAnimation {
                target: effectRoot
                property: "sunlightHaloOpacity"
                to: 0
                duration: 1450
                easing.type: Easing.InOutQuad
            }
            NumberAnimation {
                target: effectRoot
                property: "sunlightDiscOpacity"
                to: 0
                duration: 1250
                easing.type: Easing.InOutQuad
            }
            NumberAnimation {
                target: effectRoot
                property: "sunlightRayOpacity"
                to: 0
                duration: 1180
                easing.type: Easing.InOutQuad
            }
            NumberAnimation {
                target: effectRoot
                property: "sunlightHaloScale"
                to: 1.18
                duration: 1450
                easing.type: Easing.OutCubic
            }
            NumberAnimation {
                target: effectRoot
                property: "sunlightRayScale"
                to: 1.08
                duration: 1180
                easing.type: Easing.OutCubic
            }
        }
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

    Timer {
        id: lightningRepeatTimer

        repeat: false
        running: false

        onTriggered: {
            if (!effectRoot.shouldEffectsRun || !effectRoot.lightningRepeaterActive)
                return;

            effectRoot.pulseLightning();
            effectRoot.scheduleNextLightningRepeat();
        }
    }

    Rectangle {
        anchors.fill: parent
        z: 7
        color: "#ffde7a"
        opacity: effectRoot.sunlightOverlayOpacity * 0.11
        visible: opacity > 0
    }

    Item {
        width: Math.min(Math.max(effectRoot.width * 0.55, 320), 560)
        height: width
        x: effectRoot.width * effectRoot.sunlightCenterX - width / 2
        y: effectRoot.height * effectRoot.sunlightCenterY - height / 2
        z: 8
        visible: opacity > 0
        opacity: Math.max(effectRoot.sunlightHaloOpacity,
                          effectRoot.sunlightDiscOpacity,
                          effectRoot.sunlightRayOpacity)

        Rectangle {
            anchors.centerIn: parent
            width: parent.width * 0.94
            height: width
            radius: width / 2
            color: "#ffe082"
            opacity: effectRoot.sunlightHaloOpacity * 0.24
            scale: effectRoot.sunlightHaloScale
        }

        Repeater {
            model: 8

            Rectangle {
                width: Math.max(parent.width * 0.03, 16)
                height: parent.width * 0.44
                radius: width / 2
                anchors.centerIn: parent
                color: "#ffd54f"
                opacity: effectRoot.sunlightRayOpacity * (index % 2 === 0 ? 0.36 : 0.26)
                rotation: index * 45
                scale: effectRoot.sunlightRayScale
                transformOrigin: Item.Center
            }
        }

        Rectangle {
            anchors.centerIn: parent
            width: Math.min(parent.width * 0.31, 150)
            height: width
            radius: width / 2
            color: "#fff1a8"
            opacity: effectRoot.sunlightDiscOpacity * 0.95
            scale: effectRoot.sunlightDiscScale
        }

        Rectangle {
            anchors.centerIn: parent
            width: Math.min(parent.width * 0.18, 90)
            height: width
            radius: width / 2
            color: "#fff9de"
            opacity: effectRoot.sunlightDiscOpacity
            scale: effectRoot.sunlightDiscScale * 0.94
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

}
