// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15
import QtQuick.Controls 2.15
import cc.etke.komai 1.0

SpinBox {
    id: control

    FontMetrics {
        id: spinFontMetrics
        font: control.font
    }

    readonly property int indicatorHorizontalPadding: Komai.paddingSmall
    readonly property int indicatorWidth: 22 + indicatorHorizontalPadding * 2
    readonly property int minimumFieldWidth: Math.max(72,
                                                      Math.round(spinFontMetrics.averageCharacterWidth * 9))
    readonly property int controlHeight: Math.max(34, Math.round(Settings.uiFontSizePt * 2.5))
    readonly property int outerBorderWidth: activeFocus ? 2 : 1

    font.pointSize: Settings.uiFontSizePt
    wheelEnabled: activeFocus
    editable: true
    implicitWidth: leftPadding + rightPadding + minimumFieldWidth
    implicitHeight: controlHeight
    leftPadding: Komai.paddingMedium
    rightPadding: indicatorWidth * 2 + Komai.paddingMedium
    topPadding: 0
    bottomPadding: 0

    contentItem: TextInput {
        z: 2
        text: control.displayText
        clip: width < implicitWidth
        leftPadding: 0
        rightPadding: 0
        topPadding: 0
        bottomPadding: 0

        font: control.font
        color: control.enabled ? control.palette.text : control.palette.buttonText
        selectionColor: control.palette.highlight
        selectedTextColor: control.palette.highlightedText
        horizontalAlignment: Qt.AlignHCenter
        verticalAlignment: Qt.AlignVCenter

        readOnly: !control.editable
        validator: control.validator
        inputMethodHints: control.inputMethodHints
    }

    up.indicator: Rectangle {
        x: control.width - control.outerBorderWidth - width
        y: control.outerBorderWidth
        implicitWidth: control.indicatorWidth
        implicitHeight: Math.max(1, control.height - control.outerBorderWidth * 2)
        color: control.up.pressed ? Qt.darker(control.palette.alternateBase, 1.1)
                                  : control.palette.alternateBase
        border.width: 0

        Text {
            anchors.centerIn: parent
            color: control.enabled ? control.palette.text : control.palette.buttonText
            text: "+"
            font.pointSize: control.font.pointSize
            font.bold: true
        }

        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 1
            color: Komai.theme.separator
        }
    }

    down.indicator: Rectangle {
        x: control.width - control.outerBorderWidth - control.up.indicator.width - width
        y: control.outerBorderWidth
        implicitWidth: control.indicatorWidth
        implicitHeight: Math.max(1, control.height - control.outerBorderWidth * 2)
        color: control.down.pressed ? Qt.darker(control.palette.alternateBase, 1.1)
                                    : control.palette.alternateBase
        border.width: 0

        Text {
            anchors.centerIn: parent
            color: control.enabled ? control.palette.text : control.palette.buttonText
            text: "-"
            font.pointSize: control.font.pointSize
            font.bold: true
        }

        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 1
            color: Komai.theme.separator
        }
    }

    background: Rectangle {
        color: control.palette.base
        radius: Komai.paddingSmall
        border.color: control.activeFocus ? control.palette.highlight : Komai.theme.separator
        border.width: control.outerBorderWidth
    }
}
