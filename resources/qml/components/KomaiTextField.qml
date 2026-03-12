// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15
import QtQuick.Controls 2.15
import cc.etke.komai 1.0

TextField {
    id: control

    readonly property int controlHeight: Math.max(36, Math.round(Settings.uiFontSizePt * 2.7))
    readonly property color inputBackgroundColor: readOnly ? palette.alternateBase : palette.base

    font.pointSize: Settings.uiFontSizePt
    implicitHeight: controlHeight
    color: enabled ? palette.text : palette.buttonText
    placeholderTextColor: palette.buttonText
    selectionColor: palette.highlight
    selectedTextColor: palette.highlightedText
    selectByMouse: true
    padding: Komai.paddingSmall + 2
    leftPadding: Komai.paddingMedium + 2
    rightPadding: Komai.paddingMedium + 2
    topPadding: Math.max(2, Komai.paddingSmall + 2)
    bottomPadding: Math.max(2, Komai.paddingSmall + 2)

    background: Rectangle {
        color: control.enabled
            ? control.inputBackgroundColor
            : Qt.rgba(control.inputBackgroundColor.r,
                      control.inputBackgroundColor.g,
                      control.inputBackgroundColor.b,
                      0.75)
        radius: Komai.paddingSmall
        border.color: control.activeFocus ? control.palette.highlight : Komai.theme.separator
        border.width: control.activeFocus ? 2 : 1
    }
}
