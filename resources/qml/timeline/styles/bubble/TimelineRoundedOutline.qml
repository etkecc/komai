// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick

Canvas {
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

    onDashOffsetChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onBorderColorChanged: requestPaint()
    onStrokeWidthChanged: requestPaint()
    onDashPatternChanged: requestPaint()
    onCornerRadiusChanged: requestPaint()
    onInsetChanged: requestPaint()
    onVisibleChanged: {
        if (!visible && animateDashOffset)
            dashOffset = 0;
        requestPaint();
    }

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
