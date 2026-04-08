// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import cc.etke.komai

Item {
    id: root

    // Bind to the full waveformSamples list (appended every 100ms)
    property var samples: []
    property color barColor: Komai.theme.error
    property int barCount: 32
    property int barSpacing: 2
    property real barMinHeight: 2

    implicitHeight: 24

    // Peak of the visible window for normalization
    readonly property real peakLevel: {
        const total = root.samples.length;
        const offset = Math.max(0, total - root.barCount);
        let peak = 0;
        for (let i = offset; i < total; ++i) {
            if (root.samples[i] > peak)
                peak = root.samples[i];
        }
        return peak;
    }

    // Minimum peak floor so silence doesn't inflate noise to full height
    readonly property real effectivePeak: Math.max(peakLevel, 0.15)

    // Show the last barCount samples from the end of the array
    function levelForBar(barIndex) {
        const total = root.samples.length;
        if (total === 0)
            return 0;
        const offset = Math.max(0, total - root.barCount);
        const srcIndex = offset + barIndex;
        return srcIndex < total ? root.samples[srcIndex] : 0;
    }

    Row {
        anchors.fill: parent
        spacing: root.barSpacing

        Repeater {
            model: root.barCount

            Rectangle {
                required property int index
                readonly property real rawLevel: root.levelForBar(index)
                readonly property real level: rawLevel / root.effectivePeak

                width: Math.max(1, (parent.width - (root.barCount - 1) * root.barSpacing) / root.barCount)
                height: Math.max(root.barMinHeight, level * root.height)
                anchors.verticalCenter: parent.verticalCenter
                radius: Math.min(width / 2, 2)
                color: root.barColor
                opacity: 0.5 + level * 0.5

                Behavior on height {
                    NumberAnimation { duration: 80 }
                }
            }
        }
    }
}
