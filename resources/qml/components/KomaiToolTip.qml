// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Window
import cc.etke.komai 1.0

Item {
    id: control

    property var anchorItem: null
    property real anchorX: 0
    property real anchorY: 0
    property real gapX: Komai.paddingMedium
    property real gapY: Komai.paddingMedium
    property bool preferRight: true
    property bool preferBelow: false
    property bool followMouse: true
    property string text: ""
    property int delay: 0
    property bool requestedVisible: false
    property color textColor: (Window.window && Window.window.palette && Window.window.palette.toolTipText !== undefined)
        ? Window.window.palette.toolTipText
        : "white"
    property color backgroundColor: (Window.window && Window.window.palette && Window.window.palette.toolTipBase !== undefined)
        ? Window.window.palette.toolTipBase
        : "black"
    property int padding: Komai.paddingSmall
    property int leftPadding: Komai.paddingMedium
    property int rightPadding: Komai.paddingMedium
    property int topPadding: padding
    property int bottomPadding: padding
    property int viewportMargin: 6
    property bool showing: false

    // Tracks pointer position inside anchorItem for followMouse mode.
    HoverHandler {
        id: mouseTracker

        parent: control.followMouse && control.anchorItem ? control.anchorItem : control
        enabled: control.followMouse && !!control.anchorItem
    }

    readonly property point anchorPoint: {
        // In followMouse mode, anchor to the current pointer position.
        if (followMouse && mouseTracker.hovered && anchorItem) {
            const mousePos = mouseTracker.point.position;
            if (anchorItem.mapToGlobal && parent && parent.mapFromGlobal) {
                const globalPoint = anchorItem.mapToGlobal(mousePos.x, mousePos.y);
                return parent.mapFromGlobal(globalPoint.x, globalPoint.y);
            }
            if (anchorItem.mapToItem && parent)
                return anchorItem.mapToItem(parent, mousePos.x, mousePos.y);
            return mousePos;
        }

        // Static anchor fallback.
        if (anchorItem && anchorItem.mapToGlobal && parent && parent.mapFromGlobal) {
            const globalPoint = anchorItem.mapToGlobal(anchorX, anchorY);
            return parent.mapFromGlobal(globalPoint.x, globalPoint.y);
        }
        if (anchorItem && anchorItem.mapToItem && parent)
            return anchorItem.mapToItem(parent, anchorX, anchorY);
        return Qt.point(anchorX, anchorY);
    }

    // In followMouse mode, always start below-right of cursor with enough gap to clear it.
    readonly property bool _prefRight: followMouse ? true : preferRight
    readonly property bool _prefBelow: followMouse ? true : preferBelow
    readonly property real _gapX: followMouse ? Math.max(gapX, 16) : gapX
    readonly property real _gapY: followMouse ? Math.max(gapY, 24) : gapY

    parent: Overlay.overlay
    x: {
        let rawX = _prefRight ? anchorPoint.x + _gapX : anchorPoint.x - width - _gapX;
        if (parent) {
            const minX = viewportMargin;
            const maxX = parent.width - width - viewportMargin;

            if (_prefRight && rawX > maxX)
                rawX = anchorPoint.x - width - _gapX;
            else if (!_prefRight && rawX < minX)
                rawX = anchorPoint.x + _gapX;

            rawX = Math.max(minX, Math.min(rawX, maxX));
        }
        return Math.round(rawX);
    }
    y: {
        let rawY = _prefBelow ? anchorPoint.y + _gapY : anchorPoint.y - height - _gapY;
        if (parent) {
            const minY = viewportMargin;
            const maxY = parent.height - height - viewportMargin;

            if (_prefBelow && rawY > maxY)
                rawY = anchorPoint.y - height - _gapY;
            else if (!_prefBelow && rawY < minY)
                rawY = anchorPoint.y + _gapY;

            rawY = Math.max(minY, Math.min(rawY, maxY));
        }
        return Math.round(rawY);
    }
    property int maxWidth: 0
    implicitWidth: maxWidth > 0
        ? Math.min(label.implicitWidth + leftPadding + rightPadding, maxWidth)
        : label.implicitWidth + leftPadding + rightPadding
    implicitHeight: Math.ceil(label.contentHeight) + topPadding + bottomPadding
    width: implicitWidth
    height: implicitHeight
    visible: showing && text.length > 0
    z: 1000

    onRequestedVisibleChanged: {
        if (!requestedVisible) {
            delayTimer.stop();
            showing = false;
            return;
        }

        if (delay > 0) {
            showing = false;
            delayTimer.restart();
        } else {
            showing = true;
        }
    }

    onTextChanged: {
        if (text.length === 0) {
            delayTimer.stop();
            showing = false;
        } else if (requestedVisible && delay <= 0) {
            showing = true;
        }
    }

    Timer {
        id: delayTimer

        interval: control.delay
        repeat: false
        onTriggered: control.showing = control.requestedVisible && control.text.length > 0
    }

    Rectangle {
        anchors.fill: parent
        color: control.backgroundColor
        radius: Komai.paddingSmall
        border.color: Komai.theme.separator
        border.width: 1
    }

    Text {
        id: label

        text: control.text
        color: control.textColor
        font.pointSize: Settings.uiFontSizePt
        wrapMode: Text.Wrap
        x: control.leftPadding
        y: control.topPadding
        width: Math.max(0, control.width - control.leftPadding - control.rightPadding)
    }
}
