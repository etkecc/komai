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
        const effectNames = ["confetti", "rainfall", "lightning", "komaiLogo"];
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
        if (effectName === "lightning")
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
        lightningFlash.stop()
        lightningRepeatTimer.stop()
        lightningRepeaterActive = false
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
