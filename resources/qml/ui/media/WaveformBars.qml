// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import cc.etke.komai

Item {
    id: root

    property var samples: []
    property real progress: 0.0
    property color activeColor: palette.highlight
    property color inactiveColor: palette.buttonText
    property int barCount: 48
    property int barSpacing: 2
    property real barMinHeight: 2
    property bool interactive: false

    signal seekRequested(real position)

    implicitHeight: 28

    // Peak value for normalization so the tallest bar fills the height
    readonly property real peakValue: {
        let peak = 0;
        for (let i = 0; i < samples.length; ++i) {
            if (samples[i] > peak)
                peak = samples[i];
        }
        return peak;
    }

    function resampledValue(barIndex) {
        const sampleCount = root.samples.length;
        if (sampleCount === 0)
            return 0;

        const bucketSize = sampleCount / root.barCount;
        const start = Math.floor(barIndex * bucketSize);
        const end = Math.min(Math.floor((barIndex + 1) * bucketSize), sampleCount);
        if (start >= end)
            return root.samples[Math.min(start, sampleCount - 1)] || 0;

        let sum = 0;
        for (let i = start; i < end; ++i)
            sum += root.samples[i];
        return sum / (end - start);
    }

    Row {
        id: barsRow

        anchors.fill: parent
        spacing: root.barSpacing

        Repeater {
            model: root.barCount

            Rectangle {
                id: bar

                required property int index
                readonly property real rawValue: root.resampledValue(index)
                readonly property real sampleValue: root.peakValue > 0
                    ? rawValue / root.peakValue : 0
                readonly property bool active: root.barCount > 0
                    && (index / root.barCount) < root.progress

                width: Math.max(1, (barsRow.width - (root.barCount - 1) * root.barSpacing) / root.barCount)
                height: Math.max(root.barMinHeight, sampleValue * root.height)
                anchors.verticalCenter: parent.verticalCenter
                radius: Math.min(width / 2, 2)
                color: active ? root.activeColor : root.inactiveColor
                opacity: active ? 1.0 : 0.5
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        enabled: root.interactive
        cursorShape: enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: mouse => {
            const pos = mouse.x / width;
            root.seekRequested(Math.max(0, Math.min(1, pos)));
        }
        onPositionChanged: mouse => {
            if (pressed) {
                const pos = mouse.x / width;
                root.seekRequested(Math.max(0, Math.min(1, pos)));
            }
        }
    }
}
