// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick

// Every timeline bubble carries several of these outlines (pending, selected,
// focused, thread), but few are shown at once. A Canvas keeps a CPU-side image
// of its full size for as long as it exists, so the Canvas is only created
// while the outline is visible.
Item {
    id: root

    property color borderColor: "transparent"
    property real strokeWidth: 1
    property var dashPattern: []
    property real dashOffset: 0
    property real cornerRadius: 8
    property real inset: strokeWidth / 2
    property bool animateDashOffset: false
    property real animatedDashDistance: 16
    property int animationDuration: 800

    NumberAnimation on dashOffset {
        from: 0
        to: root.animatedDashDistance
        duration: root.animationDuration
        loops: Animation.Infinite
        running: root.animateDashOffset && root.visible
    }

    onVisibleChanged: {
        if (!visible && animateDashOffset)
            dashOffset = 0;
    }

    Loader {
        anchors.fill: parent
        active: root.visible

        sourceComponent: Canvas {
            id: canvas

            readonly property color borderColor: root.borderColor
            readonly property real strokeWidth: root.strokeWidth
            readonly property var dashPattern: root.dashPattern
            readonly property real dashOffset: root.dashOffset
            readonly property real cornerRadius: root.cornerRadius
            readonly property real inset: root.inset

            onDashOffsetChanged: requestPaint()
            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            onBorderColorChanged: requestPaint()
            onStrokeWidthChanged: requestPaint()
            onDashPatternChanged: requestPaint()
            onCornerRadiusChanged: requestPaint()
            onInsetChanged: requestPaint()

            onPaint: {
                var ctx = getContext("2d");
                ctx.clearRect(0, 0, width, height);
                ctx.strokeStyle = borderColor;
                ctx.lineWidth = strokeWidth;
                ctx.setLineDash(dashPattern && dashPattern.length ? dashPattern : []);
                ctx.lineDashOffset = (dashPattern && dashPattern.length) ? -dashOffset : 0;
                ctx.beginPath();
                ctx.roundedRect(inset, inset, width - 2 * inset, height - 2 * inset, cornerRadius, cornerRadius);
                ctx.stroke();
            }
        }
    }
}
