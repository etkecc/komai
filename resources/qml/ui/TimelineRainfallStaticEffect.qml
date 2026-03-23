// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15

Item {
    id: root

    property int durationMs: 800
    property real effectOpacity: 0
    readonly property real activeOpacity: effectOpacity

    anchors.fill: parent
    visible: effectOpacity > 0

    function trigger()
    {
        streakRepeater.model = generateStreaks();
        effectOpacity = 1;
    }

    function reset()
    {
        effectOpacity = 0;
    }

    function generateStreaks()
    {
        var streaks = [];
        var count = 70;
        for (var i = 0; i < count; ++i) {
            streaks.push({
                "xRatio": Math.random(),
                "yRatio": Math.random(),
                "heightRatio": 0.03 + Math.random() * 0.05,
                "opacity": 0.25 + Math.random() * 0.45
            });
        }
        return streaks;
    }

    Rectangle {
        anchors.fill: parent
        color: "#1a5276"
        opacity: root.effectOpacity * 0.06
        visible: opacity > 0
    }

    Repeater {
        id: streakRepeater

        model: []

        Rectangle {
            x: root.width * modelData.xRatio
            y: root.height * modelData.yRatio
            width: Math.max(2, root.width * 0.002)
            height: root.height * modelData.heightRatio
            radius: width / 2
            color: "#0099ff"
            opacity: root.effectOpacity * modelData.opacity
        }
    }
}
