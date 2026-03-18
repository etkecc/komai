// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Window 2.15
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
    readonly property point anchorPoint: {
        if (anchorItem && anchorItem.mapToGlobal && parent && parent.mapFromGlobal) {
            const globalPoint = anchorItem.mapToGlobal(anchorX, anchorY);
            return parent.mapFromGlobal(globalPoint.x, globalPoint.y);
        }

        if (anchorItem && anchorItem.mapToItem && parent)
            return anchorItem.mapToItem(parent, anchorX, anchorY);

        return Qt.point(anchorX, anchorY);
    }

    parent: Overlay.overlay
    x: {
        let rawX = preferRight ? anchorPoint.x + gapX : anchorPoint.x - width - gapX;
        if (parent) {
            const minX = viewportMargin;
            const maxX = parent.width - width - viewportMargin;

            if (preferRight && rawX > maxX)
                rawX = anchorPoint.x - width - gapX;
            else if (!preferRight && rawX < minX)
                rawX = anchorPoint.x + gapX;

            rawX = Math.max(minX, Math.min(rawX, maxX));
        }
        return Math.round(rawX);
    }
    y: {
        let rawY = preferBelow ? anchorPoint.y + gapY : anchorPoint.y - height - gapY;
        if (parent) {
            const minY = viewportMargin;
            const maxY = parent.height - height - viewportMargin;

            if (preferBelow && rawY > maxY)
                rawY = anchorPoint.y - height - gapY;
            else if (!preferBelow && rawY < minY)
                rawY = anchorPoint.y + gapY;

            rawY = Math.max(minY, Math.min(rawY, maxY));
        }
        return Math.round(rawY);
    }
    implicitWidth: label.implicitWidth + leftPadding + rightPadding
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
        wrapMode: Text.WrapAnywhere
        x: control.leftPadding
        y: control.topPadding
        width: Math.max(0, control.width - control.leftPadding - control.rightPadding)
    }
}
