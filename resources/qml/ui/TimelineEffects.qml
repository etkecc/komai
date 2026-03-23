// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15

Item {
    id: effectRoot

    readonly property var supportedEffectNames: ["confetti", "sunlight", "love", "rainfall", "lightning", "komaiLogo"]
    readonly property var effectEmitters: ({
        "confetti": particlesLoader.item ? particlesLoader.item.confettiEmitter : null,
        "rainfall": particlesLoader.item ? particlesLoader.item.rainfallEmitter : null
    })
    readonly property var effectPulseScales: ({
        "confetti": 2.0,
        "rainfall": 3.3
    })
    readonly property var overlayEffects: ({
        "sunlight": sunlightEffect,
        "love": loveEffect,
        "lightning": lightningEffect,
        "komaiLogo": komaiEffect
    })
    readonly property var staticParticleEffects: ({
        "confetti": confettiStaticEffect,
        "rainfall": rainfallStaticEffect
    })
    readonly property int maxEffectDuration: {
        var max = 0;
        for (let i = 0; i < supportedEffectNames.length; ++i) {
            const duration = effectDuration(supportedEffectNames[i]);
            if (duration > max)
                max = duration;
        }
        return max;
    }

    required property bool shouldEffectsRun
    property bool animationsEnabled: true

    visible: effectRoot.shouldEffectsRun

    function isParticleEffect(effectName)
    {
        return effectName === "confetti" || effectName === "rainfall";
    }

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
        if (!isParticleEffect(effectName))
            return 0;

        if (!animationsEnabled)
            return 0;

        const scale = effectPulseScales[effectName] || 1.0;
        let pulseDuration = effectRoot.height * scale;
        if (effectName === "rainfall")
            pulseDuration = Math.min(pulseDuration, 1200);
        return pulseDuration;
    }

    function effectDuration(effectName)
    {
        if (!animationsEnabled) {
            const staticEffect = staticParticleEffects[effectName];
            if (staticEffect)
                return staticEffect.durationMs;

            if (overlayEffects[effectName])
                return 800;

            return 0;
        }

        const overlay = overlayEffects[effectName];
        if (overlay)
            return overlay.durationMs;

        const emitter = effectEmitters[effectName];
        if (!emitter)
            return 0;

        return pulseDurationForEffect(effectName) + emitter.lifeSpan;
    }

    function pulseEffects(effectNames)
    {
        if (!effectNames)
            return;

        let hasParticleEffects = false;
        const repeatLightning = effectNames.indexOf("rainfall") !== -1
                              && effectNames.indexOf("lightning") !== -1;

        for (let i = 0; i < effectNames.length; ++i) {
            if (isParticleEffect(effectNames[i])) {
                hasParticleEffects = true;
                break;
            }
        }

        if (hasParticleEffects && animationsEnabled)
            ensureParticleLayer();

        for (let i = 0; i < effectNames.length; ++i)
            pulseEffect(effectNames[i], repeatLightning);
    }

    function pulseEffect(effectName, repeatLightning)
    {
        const overlay = overlayEffects[effectName];
        if (overlay) {
            if (effectName === "lightning")
                overlay.trigger(repeatLightning);
            else
                overlay.trigger();
            return;
        }

        if (!animationsEnabled) {
            const staticEffect = staticParticleEffects[effectName];
            if (staticEffect) {
                staticEffect.trigger();
                return;
            }
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

    function resetOverlays()
    {
        lightningEffect.reset();
        sunlightEffect.reset();
        loveEffect.reset();
        komaiEffect.reset();
        confettiStaticEffect.reset();
        rainfallStaticEffect.reset();
    }

    function removeParticles()
    {
        resetOverlays();
        if (animationsEnabled) {
            particlesLoader.active = false;
            Qt.callLater(function() {
                particlesLoader.active = true;
            });
        }
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

    TimelineConfettiStaticEffect {
        id: confettiStaticEffect

        anchors.fill: parent
        z: 5
    }

    TimelineRainfallStaticEffect {
        id: rainfallStaticEffect

        anchors.fill: parent
        z: 6
    }

    TimelineKomaiEffect {
        id: komaiEffect

        anchors.fill: parent
        animationsEnabled: effectRoot.animationsEnabled
        z: 8
    }

    TimelineSunlightEffect {
        id: sunlightEffect

        anchors.fill: parent
        animationsEnabled: effectRoot.animationsEnabled
        z: 9
    }

    TimelineLoveEffect {
        id: loveEffect

        anchors.fill: parent
        animationsEnabled: effectRoot.animationsEnabled
        z: 10
    }

    TimelineLightningEffect {
        id: lightningEffect

        anchors.fill: parent
        animationsEnabled: effectRoot.animationsEnabled
        z: 11
    }
}
