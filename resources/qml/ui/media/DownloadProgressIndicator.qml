// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick

// Determinate media-download indicator: a dark disc with a progress ring,
// a centered percentage, and the Komai logo pulsing at the leading edge of
// the arc — the determinate sibling of the indeterminate Spinner.
Rectangle {
    id: indicator

    // Download progress in [0, 1]; out-of-range values are clamped. Animated
    // between updates so 150ms polling steps read as continuous motion.
    property real progress: 0

    readonly property real clamped: Math.max(0, Math.min(1, progress))
    readonly property real ringLineWidth: Math.max(3, width * 0.05)
    readonly property real ringRadius: width / 2 - ringLineWidth

    width: 84
    height: width
    radius: width / 2
    color: Qt.rgba(0, 0, 0, 0.6)

    Behavior on progress {
        enabled: Settings.uiMotionAnimationsEnabled
        NumberAnimation { duration: 150 }
    }

    onClampedChanged: progressRing.requestPaint()
    onVisibleChanged: if (visible) progressRing.requestPaint()

    Canvas {
        id: progressRing

        anchors.fill: parent
        onPaint: {
            const ctx = getContext("2d");
            ctx.reset();
            ctx.lineWidth = indicator.ringLineWidth;
            ctx.lineCap = "round";
            ctx.strokeStyle = Qt.rgba(1, 1, 1, 0.25).toString();
            ctx.beginPath();
            ctx.arc(width / 2, height / 2, indicator.ringRadius, 0, 2 * Math.PI);
            ctx.stroke();
            ctx.strokeStyle = "white";
            ctx.beginPath();
            ctx.arc(width / 2, height / 2, indicator.ringRadius,
                    -Math.PI / 2, -Math.PI / 2 + 2 * Math.PI * indicator.clamped);
            ctx.stroke();
        }
    }

    Text {
        anchors.centerIn: parent
        color: "white"
        font.pixelSize: indicator.width * 0.26
        text: Math.round(indicator.clamped * 100) + "%"
    }

    // The Komai logo rides the leading edge of the arc, pulsing like the
    // indeterminate Spinner does, so the percentage owns the center while
    // the brand mark shows where the ring is filling.
    Image {
        id: progressLogo

        readonly property real angle: -Math.PI / 2 + 2 * Math.PI * indicator.clamped

        width: parent.width * 0.3
        height: width
        x: parent.width / 2 + indicator.ringRadius * Math.cos(angle) - width / 2
        y: parent.height / 2 + indicator.ringRadius * Math.sin(angle) - height / 2
        source: "qrc:/logos/komai.svg"
        sourceSize.width: width * 2
        sourceSize.height: height * 2
        fillMode: Image.PreserveAspectFit
        smooth: true

        SequentialAnimation {
            loops: Animation.Infinite
            running: progressLogo.visible && Settings.uiMotionAnimationsEnabled

            NumberAnimation {
                target: progressLogo
                property: "scale"
                from: 1.0
                to: 1.2
                duration: 400
                easing.type: Easing.OutQuad
            }
            NumberAnimation {
                target: progressLogo
                property: "scale"
                from: 1.2
                to: 1.0
                duration: 400
                easing.type: Easing.InQuad
            }

            onRunningChanged: {
                if (!running)
                    progressLogo.scale = 1.0;
            }
        }
    }
}
