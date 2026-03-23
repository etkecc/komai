// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15

Item {
    id: root

    property int durationMs: 800
    property real effectOpacity: 0
    readonly property real activeOpacity: effectOpacity
    readonly property var confettiColors: [
        "#ff4081", "#536dfe", "#ffab40", "#69f0ae",
        "#ea80fc", "#ff5252", "#40c4ff", "#ffd740",
        "#b2ff59", "#ff80ab", "#448aff", "#ff6e40"
    ]

    anchors.fill: parent
    visible: effectOpacity > 0

    function trigger()
    {
        pieceRepeater.model = generatePieces();
        effectOpacity = 1;
    }

    function reset()
    {
        effectOpacity = 0;
    }

    function generatePieces()
    {
        var pieces = [];
        var count = 45;
        for (var i = 0; i < count; ++i) {
            pieces.push({
                "xRatio": Math.random(),
                "yRatio": Math.random(),
                "rotation": Math.random() * 360,
                "sizeRatio": 0.012 + Math.random() * 0.012,
                "aspectRatio": 0.4 + Math.random() * 0.6,
                "colorIndex": Math.floor(Math.random() * confettiColors.length)
            });
        }
        return pieces;
    }

    Repeater {
        id: pieceRepeater

        model: []

        Rectangle {
            x: root.width * modelData.xRatio
            y: root.height * modelData.yRatio
            width: Math.max(root.width * modelData.sizeRatio, 8)
            height: width * modelData.aspectRatio
            rotation: modelData.rotation
            radius: Math.min(width, height) * 0.2
            color: root.confettiColors[modelData.colorIndex]
            opacity: root.effectOpacity * (0.7 + Math.random() * 0.3)
        }
    }
}
