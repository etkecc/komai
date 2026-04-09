// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Particles

Item {
    id: effectRoot

    required property bool shouldEffectsRun
    property bool animationsEnabled: true

    property real confettiStaticOpacity: 0
    property real rainfallStaticOpacity: 0
    property var confettiStaticPieces: []
    property var rainfallStaticStreaks: []

    property real sunlightOverlayOpacity: 0
    property real sunlightHaloOpacity: 0
    property real sunlightDiscOpacity: 0
    property real sunlightRayOpacity: 0
    property real sunlightScale: 0.7
    property real sunlightCenterX: 0.82
    property real sunlightCenterY: 0.14

    property real loveOverlayOpacity: 0
    property real loveHaloOpacity: 0
    property real loveRingOpacity: 0
    property real loveHeartOpacity: 0
    property real loveSparkleOpacity: 0
    property real loveScale: 0.72
    property real loveLift: 18

    property real lightningFlashOpacity: 0
    property real lightningBoltX: 0.5
    property real lightningBoltScale: 1.0
    property real lightningBoltRotation: 0
    property bool lightningRepeating: false

    property real komaiOverlayOpacity: 0
    property real komaiHaloOpacity: 0
    property real komaiPetalOpacity: 0
    property real komaiRingOpacity: 0
    property real komaiGlowOpacity: 0
    property real komaiScale: 0.62

    readonly property var supportedEffectNames: ["confetti", "sunlight", "love", "rainfall", "lightning", "komaiLogo"]
    readonly property int maxEffectDuration: 2200

    anchors.fill: parent
    visible: shouldEffectsRun

    function durationForEffects(effectNames)
    {
        if (!effectNames || effectNames.length === 0)
            return maxEffectDuration;

        let duration = 0;
        for (let i = 0; i < effectNames.length; ++i)
            duration = Math.max(duration, effectDuration(effectNames[i]));
        return duration > 0 ? duration : maxEffectDuration;
    }

    function effectDuration(effectName)
    {
        switch (effectName) {
        case "confetti":
            return animationsEnabled ? 2200 : 900;
        case "rainfall":
            return animationsEnabled ? 1800 : 800;
        case "sunlight":
            return 2200;
        case "love":
            return 1700;
        case "lightning":
            return 1150;
        case "komaiLogo":
            return 1740;
        default:
            return 0;
        }
    }

    function pulseEffects(effectNames)
    {
        if (!effectNames || effectNames.length === 0)
            return;

        particleSystem.running = true;

        const repeatLightning = effectNames.indexOf("rainfall") !== -1
            && effectNames.indexOf("lightning") !== -1;

        for (let i = 0; i < effectNames.length; ++i) {
            switch (effectNames[i]) {
            case "confetti":
                triggerConfetti();
                break;
            case "rainfall":
                triggerRainfall();
                break;
            case "sunlight":
                triggerSunlight();
                break;
            case "love":
                triggerLove();
                break;
            case "lightning":
                triggerLightning(repeatLightning);
                break;
            case "komaiLogo":
                triggerKomai();
                break;
            default:
                console.warn("Unknown timeline effect:", effectNames[i]);
            }
        }
    }

    function resetOverlays()
    {
        confettiStaticOpacity = 0;
        rainfallStaticOpacity = 0;
        confettiStaticPieces = [];
        rainfallStaticStreaks = [];

        sunlightBurst.stop();
        loveBurst.stop();
        lightningFlash.stop();
        lightningRepeatTimer.stop();
        komaiBurst.stop();

        sunlightOverlayOpacity = 0;
        sunlightHaloOpacity = 0;
        sunlightDiscOpacity = 0;
        sunlightRayOpacity = 0;
        sunlightScale = 0.7;

        loveOverlayOpacity = 0;
        loveHaloOpacity = 0;
        loveRingOpacity = 0;
        loveHeartOpacity = 0;
        loveSparkleOpacity = 0;
        loveScale = 0.72;
        loveLift = 18;

        lightningFlashOpacity = 0;
        lightningRepeating = false;

        komaiOverlayOpacity = 0;
        komaiHaloOpacity = 0;
        komaiPetalOpacity = 0;
        komaiRingOpacity = 0;
        komaiGlowOpacity = 0;
        komaiScale = 0.62;
    }

    function removeParticles()
    {
        resetOverlays();
        particleSystem.reset();
        particleSystem.running = false;
    }

    function triggerConfetti()
    {
        if (!animationsEnabled) {
            confettiStaticPieces = generateConfettiPieces();
            confettiStaticOpacity = 1;
            return;
        }

        confettiEmitter.pulse(Math.min(Math.max(height * 0.45, 260), 480));
    }

    function triggerRainfall()
    {
        if (!animationsEnabled) {
            rainfallStaticStreaks = generateRainStreaks();
            rainfallStaticOpacity = 1;
            return;
        }

        rainfallEmitter.pulse(Math.min(Math.max(height * 1.8, 450), 1200));
    }

    function triggerSunlight()
    {
        sunlightCenterX = 0.78 + Math.random() * 0.1;
        sunlightCenterY = 0.08 + Math.random() * 0.08;
        sunlightBurst.stop();
        if (!animationsEnabled) {
            sunlightOverlayOpacity = 1;
            sunlightHaloOpacity = 1;
            sunlightDiscOpacity = 1;
            sunlightRayOpacity = 0.72;
            sunlightScale = 1;
            return;
        }

        sunlightOverlayOpacity = 0;
        sunlightHaloOpacity = 0;
        sunlightDiscOpacity = 0;
        sunlightRayOpacity = 0;
        sunlightScale = 0.7;
        sunlightBurst.start();
    }

    function triggerLove()
    {
        loveBurst.stop();
        if (!animationsEnabled) {
            loveOverlayOpacity = 1;
            loveHaloOpacity = 1;
            loveRingOpacity = 0.74;
            loveHeartOpacity = 1;
            loveSparkleOpacity = 1;
            loveScale = 1;
            loveLift = 0;
            return;
        }

        loveOverlayOpacity = 0;
        loveHaloOpacity = 0;
        loveRingOpacity = 0;
        loveHeartOpacity = 0;
        loveSparkleOpacity = 0;
        loveScale = 0.72;
        loveLift = 18;
        loveBurst.start();
    }

    function strikeLightning()
    {
        lightningBoltX = 0.18 + Math.random() * 0.64;
        lightningBoltScale = 0.9 + Math.random() * 0.35;
        lightningBoltRotation = -18 + Math.random() * 36;
        lightningFlash.restart();
    }

    function triggerLightning(repeatStrikes)
    {
        lightningRepeatTimer.stop();
        lightningRepeating = !!repeatStrikes && animationsEnabled;
        if (!animationsEnabled) {
            lightningBoltX = 0.18 + Math.random() * 0.64;
            lightningBoltScale = 0.9 + Math.random() * 0.35;
            lightningBoltRotation = -18 + Math.random() * 36;
            lightningFlashOpacity = 0.95;
            return;
        }

        strikeLightning();
        if (lightningRepeating) {
            lightningRepeatTimer.interval = 500 + Math.random() * 650;
            lightningRepeatTimer.start();
        }
    }

    function triggerKomai()
    {
        komaiBurst.stop();
        if (!animationsEnabled) {
            komaiOverlayOpacity = 1;
            komaiHaloOpacity = 1;
            komaiPetalOpacity = 1;
            komaiRingOpacity = 0.8;
            komaiGlowOpacity = 1;
            komaiScale = 1;
            return;
        }

        komaiOverlayOpacity = 0;
        komaiHaloOpacity = 0;
        komaiPetalOpacity = 0;
        komaiRingOpacity = 0;
        komaiGlowOpacity = 0;
        komaiScale = 0.62;
        komaiBurst.start();
    }

    function generateConfettiPieces()
    {
        const pieces = [];
        for (let i = 0; i < 45; ++i) {
            pieces.push({
                "xRatio": Math.random(),
                "yRatio": Math.random(),
                "rotation": Math.random() * 360,
                "sizeRatio": 0.012 + Math.random() * 0.012,
                "aspectRatio": 0.4 + Math.random() * 0.6,
                "color": ["#ff4081", "#536dfe", "#ffab40", "#69f0ae", "#ea80fc", "#ff5252", "#40c4ff", "#ffd740"][Math.floor(Math.random() * 8)]
            });
        }
        return pieces;
    }

    function generateRainStreaks()
    {
        const streaks = [];
        for (let i = 0; i < 70; ++i) {
            streaks.push({
                "xRatio": Math.random(),
                "yRatio": Math.random(),
                "heightRatio": 0.03 + Math.random() * 0.05,
                "opacity": 0.25 + Math.random() * 0.45
            });
        }
        return streaks;
    }

    ParticleSystem {
        id: particleSystem

        anchors.fill: parent
        running: false
    }

    Emitter {
        id: confettiEmitter

        group: "confetti"
        width: effectRoot.width * 0.75
        enabled: false
        anchors.horizontalCenter: effectRoot.horizontalCenter
        y: effectRoot.height
        emitRate: Math.min(400 * Math.sqrt(effectRoot.width * effectRoot.height) / 870, 1000)
        lifeSpan: 2200
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
        xVector: PointDirection { x: 1; y: 0; xVariation: 0.2; yVariation: 0.2 }
        yVector: PointDirection { x: 0; y: 0.5; xVariation: 0.2; yVariation: 0.2 }
    }

    Gravity {
        system: particleSystem
        groups: ["confetti"]
        anchors.fill: parent
        magnitude: 350
        angle: 90
    }

    Emitter {
        id: rainfallEmitter

        group: "rain"
        width: effectRoot.width * 1.15
        enabled: false
        anchors.horizontalCenter: effectRoot.horizontalCenter
        y: -60
        emitRate: Math.min(effectRoot.width / 1.5, 950)
        lifeSpan: 1800
        system: particleSystem
        velocity: PointDirection { x: 0; y: 700; xVariation: 0; yVariation: 180 }
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
        xVector: PointDirection { x: 0.01; y: 0 }
        yVector: PointDirection { x: 0; y: 5 }
    }

    Rectangle {
        anchors.fill: parent
        color: "#1a5276"
        opacity: rainfallStaticOpacity * 0.06
        visible: opacity > 0
    }

    Repeater {
        model: confettiStaticPieces

        Rectangle {
            x: effectRoot.width * modelData.xRatio
            y: effectRoot.height * modelData.yRatio
            width: Math.max(effectRoot.width * modelData.sizeRatio, 8)
            height: width * modelData.aspectRatio
            rotation: modelData.rotation
            radius: Math.min(width, height) * 0.2
            color: modelData.color
            opacity: effectRoot.confettiStaticOpacity * 0.85
            visible: opacity > 0
        }
    }

    Repeater {
        model: rainfallStaticStreaks

        Rectangle {
            x: effectRoot.width * modelData.xRatio
            y: effectRoot.height * modelData.yRatio
            width: Math.max(2, effectRoot.width * 0.002)
            height: effectRoot.height * modelData.heightRatio
            radius: width / 2
            color: "#0099ff"
            opacity: effectRoot.rainfallStaticOpacity * modelData.opacity
            visible: opacity > 0
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#ffde7a"
        opacity: sunlightOverlayOpacity * 0.11
        visible: opacity > 0
    }

    Item {
        width: Math.min(Math.max(effectRoot.width * 0.55, 320), 560)
        height: width
        x: effectRoot.width * sunlightCenterX - width / 2
        y: effectRoot.height * sunlightCenterY - height / 2
        visible: opacity > 0
        opacity: Math.max(sunlightHaloOpacity, sunlightDiscOpacity, sunlightRayOpacity)

        Rectangle {
            anchors.centerIn: parent
            width: parent.width * 0.94
            height: width
            radius: width / 2
            color: "#ffe082"
            opacity: sunlightHaloOpacity * 0.24
            scale: sunlightScale * 1.12
        }

        Repeater {
            model: 8

            Rectangle {
                width: Math.max(parent.width * 0.03, 16)
                height: parent.width * 0.44
                radius: width / 2
                anchors.centerIn: parent
                color: "#ffd54f"
                opacity: sunlightRayOpacity * (index % 2 === 0 ? 0.36 : 0.26)
                rotation: index * 45
                scale: sunlightScale
                transformOrigin: Item.Center
            }
        }

        Rectangle {
            anchors.centerIn: parent
            width: Math.min(parent.width * 0.31, 150)
            height: width
            radius: width / 2
            color: "#fff1a8"
            opacity: sunlightDiscOpacity * 0.95
            scale: sunlightScale
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#ffc1d9"
        opacity: loveOverlayOpacity * 0.08
        visible: opacity > 0
    }

    Item {
        width: Math.min(Math.max(effectRoot.width * 0.38, 240), 360)
        height: Math.round(width * 0.88)
        x: effectRoot.width * 0.5 - width / 2
        y: Math.min(Math.max(effectRoot.height * 0.2, 44), effectRoot.height * 0.3)
        visible: opacity > 0
        opacity: Math.max(loveHaloOpacity, loveRingOpacity, loveHeartOpacity, loveSparkleOpacity)

        Rectangle {
            anchors.centerIn: parent
            width: parent.width * 0.9
            height: width
            radius: width / 2
            color: "#ffd7e6"
            opacity: loveHaloOpacity * 0.32
            scale: loveScale * 1.12
        }

        Rectangle {
            anchors.centerIn: parent
            width: parent.width * 0.66
            height: width
            radius: width / 2
            color: "transparent"
            border.width: Math.max(2, Math.round(parent.width * 0.012))
            border.color: "#ffb2c9"
            opacity: loveRingOpacity * 0.76
            scale: loveScale * 1.18
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
                y: parent.height * modelData.yRatio - height / 2 + effectRoot.loveLift * modelData.rise
                rotation: modelData.rotation
                opacity: effectRoot.loveHeartOpacity
                scale: effectRoot.loveScale

                Rectangle {
                    anchors.horizontalCenter: parent.horizontalCenter
                    y: parent.height * 0.3
                    width: parent.width * 0.56
                    height: parent.height * 0.56
                    radius: parent.width * 0.08
                    rotation: 45
                    color: modelData.color
                }
                Rectangle {
                    x: parent.width * 0.1
                    y: parent.height * 0.06
                    width: parent.width * 0.46
                    height: width
                    radius: width / 2
                    color: modelData.color
                }
                Rectangle {
                    x: parent.width * 0.44
                    y: parent.height * 0.06
                    width: parent.width * 0.46
                    height: width
                    radius: width / 2
                    color: modelData.color
                }
            }
        }

        Repeater {
            model: [
                { "xRatio": 0.18, "yRatio": 0.14, "rotation": 0, "scale": 1.0 },
                { "xRatio": 0.84, "yRatio": 0.2, "rotation": 30, "scale": 0.78 },
                { "xRatio": 0.12, "yRatio": 0.7, "rotation": -18, "scale": 0.72 },
                { "xRatio": 0.88, "yRatio": 0.78, "rotation": 12, "scale": 0.66 }
            ]

            Item {
                width: parent.width * 0.12
                height: width
                x: parent.width * modelData.xRatio - width / 2
                y: parent.height * modelData.yRatio - height / 2
                rotation: modelData.rotation
                opacity: effectRoot.loveSparkleOpacity
                scale: effectRoot.loveScale * modelData.scale

                Rectangle {
                    anchors.centerIn: parent
                    width: parent.width * 0.18
                    height: parent.height
                    radius: width / 2
                    color: "#fff8dd"
                }
                Rectangle {
                    anchors.centerIn: parent
                    width: parent.width
                    height: parent.height * 0.18
                    radius: height / 2
                    color: "#fff8dd"
                }
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "#fff6cf"
        opacity: lightningFlashOpacity * 0.22
        visible: opacity > 0
    }

    Item {
        width: 120
        height: Math.min(effectRoot.height * 0.55, 240)
        x: effectRoot.width * lightningBoltX - width / 2
        y: 18
        visible: opacity > 0
        opacity: lightningFlashOpacity
        scale: lightningBoltScale
        rotation: lightningBoltRotation

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

    Rectangle {
        anchors.fill: parent
        color: "#ffd7e6"
        opacity: komaiOverlayOpacity * 0.09
        visible: opacity > 0
    }

    Item {
        width: Math.min(Math.max(effectRoot.width * 0.39, 250), 360)
        height: width
        x: effectRoot.width * 0.5 - width / 2
        y: Math.min(Math.max(effectRoot.height * 0.14, 34), effectRoot.height * 0.24)
        visible: opacity > 0
        opacity: Math.max(komaiHaloOpacity, komaiPetalOpacity, komaiRingOpacity, komaiGlowOpacity)
        scale: komaiScale

        Rectangle {
            anchors.centerIn: parent
            width: parent.width * 0.96
            height: width
            radius: width / 2
            color: "#ffe4ed"
            opacity: komaiHaloOpacity * 0.38
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
                    opacity: komaiPetalOpacity * 0.88
                    transformOrigin: Item.Bottom
                }
            }
        }

        Rectangle {
            anchors.centerIn: parent
            width: parent.width * 0.34
            height: width
            radius: width / 2
            color: "transparent"
            border.width: Math.max(3, Math.round(parent.width * 0.014))
            border.color: "#ffc6d8"
            opacity: komaiRingOpacity * 0.88
            scale: 1.08
        }

        Rectangle {
            anchors.centerIn: parent
            width: parent.width * 0.18
            height: width
            radius: width / 2
            color: "#fff6d8"
            opacity: komaiGlowOpacity
        }

        Image {
            anchors.centerIn: parent
            width: parent.width * 0.42
            height: width
            fillMode: Image.PreserveAspectFit
            source: "qrc:/logos/komai.svg"
            smooth: true
            mipmap: true
            opacity: Math.max(komaiGlowOpacity, komaiHaloOpacity * 0.92)
            visible: opacity > 0
            z: 2
        }
    }

    SequentialAnimation {
        id: sunlightBurst
        running: false

        ParallelAnimation {
            NumberAnimation { target: effectRoot; property: "sunlightOverlayOpacity"; to: 1; duration: 220; easing.type: Easing.OutCubic }
            NumberAnimation { target: effectRoot; property: "sunlightHaloOpacity"; to: 1; duration: 260; easing.type: Easing.OutCubic }
            NumberAnimation { target: effectRoot; property: "sunlightDiscOpacity"; to: 1; duration: 230; easing.type: Easing.OutCubic }
            NumberAnimation { target: effectRoot; property: "sunlightRayOpacity"; to: 0.72; duration: 240; easing.type: Easing.OutCubic }
            NumberAnimation { target: effectRoot; property: "sunlightScale"; to: 1.0; duration: 320; easing.type: Easing.OutBack }
        }
        PauseAnimation { duration: 260 }
        ParallelAnimation {
            NumberAnimation { target: effectRoot; property: "sunlightOverlayOpacity"; to: 0; duration: 1400; easing.type: Easing.InOutQuad }
            NumberAnimation { target: effectRoot; property: "sunlightHaloOpacity"; to: 0; duration: 1450; easing.type: Easing.InOutQuad }
            NumberAnimation { target: effectRoot; property: "sunlightDiscOpacity"; to: 0; duration: 1250; easing.type: Easing.InOutQuad }
            NumberAnimation { target: effectRoot; property: "sunlightRayOpacity"; to: 0; duration: 1180; easing.type: Easing.InOutQuad }
        }
    }

    SequentialAnimation {
        id: loveBurst
        running: false

        ParallelAnimation {
            NumberAnimation { target: effectRoot; property: "loveOverlayOpacity"; to: 1; duration: 160; easing.type: Easing.OutCubic }
            NumberAnimation { target: effectRoot; property: "loveHaloOpacity"; to: 1; duration: 220; easing.type: Easing.OutCubic }
            NumberAnimation { target: effectRoot; property: "loveRingOpacity"; to: 0.74; duration: 240; easing.type: Easing.OutCubic }
            NumberAnimation { target: effectRoot; property: "loveHeartOpacity"; to: 1; duration: 260; easing.type: Easing.OutCubic }
            NumberAnimation { target: effectRoot; property: "loveSparkleOpacity"; to: 1; duration: 180; easing.type: Easing.OutCubic }
            NumberAnimation { target: effectRoot; property: "loveScale"; to: 1; duration: 320; easing.type: Easing.OutBack }
            NumberAnimation { target: effectRoot; property: "loveLift"; to: 0; duration: 320; easing.type: Easing.OutCubic }
        }
        PauseAnimation { duration: 220 }
        ParallelAnimation {
            NumberAnimation { target: effectRoot; property: "loveOverlayOpacity"; to: 0; duration: 900; easing.type: Easing.InOutQuad }
            NumberAnimation { target: effectRoot; property: "loveHaloOpacity"; to: 0; duration: 980; easing.type: Easing.InOutQuad }
            NumberAnimation { target: effectRoot; property: "loveRingOpacity"; to: 0; duration: 760; easing.type: Easing.OutCubic }
            NumberAnimation { target: effectRoot; property: "loveHeartOpacity"; to: 0; duration: 940; easing.type: Easing.InOutQuad }
            NumberAnimation { target: effectRoot; property: "loveSparkleOpacity"; to: 0; duration: 700; easing.type: Easing.OutCubic }
            NumberAnimation { target: effectRoot; property: "loveLift"; to: -24; duration: 940; easing.type: Easing.OutCubic }
        }
    }

    SequentialAnimation {
        id: lightningFlash
        running: false

        NumberAnimation { target: effectRoot; property: "lightningFlashOpacity"; to: 0.95; duration: 45 }
        PauseAnimation { duration: 35 }
        NumberAnimation { target: effectRoot; property: "lightningFlashOpacity"; to: 0.3; duration: 55 }
        PauseAnimation { duration: 45 }
        NumberAnimation { target: effectRoot; property: "lightningFlashOpacity"; to: 0.8; duration: 40 }
        NumberAnimation { target: effectRoot; property: "lightningFlashOpacity"; to: 0; duration: 220 }
    }

    Timer {
        id: lightningRepeatTimer
        repeat: false
        running: false

        onTriggered: {
            if (!effectRoot.lightningRepeating)
                return;

            effectRoot.strikeLightning();
            interval = 500 + Math.random() * 650;
            start();
        }
    }

    SequentialAnimation {
        id: komaiBurst
        running: false

        ParallelAnimation {
            NumberAnimation { target: effectRoot; property: "komaiOverlayOpacity"; to: 1; duration: 140; easing.type: Easing.OutCubic }
            NumberAnimation { target: effectRoot; property: "komaiHaloOpacity"; to: 1; duration: 180; easing.type: Easing.OutCubic }
            NumberAnimation { target: effectRoot; property: "komaiPetalOpacity"; to: 1; duration: 210; easing.type: Easing.OutCubic }
            NumberAnimation { target: effectRoot; property: "komaiRingOpacity"; to: 0.8; duration: 200; easing.type: Easing.OutCubic }
            NumberAnimation { target: effectRoot; property: "komaiGlowOpacity"; to: 1; duration: 180; easing.type: Easing.OutCubic }
            NumberAnimation { target: effectRoot; property: "komaiScale"; to: 1; duration: 260; easing.type: Easing.OutBack }
        }
        PauseAnimation { duration: 280 }
        ParallelAnimation {
            NumberAnimation { target: effectRoot; property: "komaiOverlayOpacity"; to: 0; duration: 980; easing.type: Easing.InOutQuad }
            NumberAnimation { target: effectRoot; property: "komaiHaloOpacity"; to: 0; duration: 1120; easing.type: Easing.InOutQuad }
            NumberAnimation { target: effectRoot; property: "komaiPetalOpacity"; to: 0; duration: 1080; easing.type: Easing.InOutQuad }
            NumberAnimation { target: effectRoot; property: "komaiRingOpacity"; to: 0; duration: 820; easing.type: Easing.OutCubic }
            NumberAnimation { target: effectRoot; property: "komaiGlowOpacity"; to: 0; duration: 720; easing.type: Easing.OutCubic }
        }
    }
}
