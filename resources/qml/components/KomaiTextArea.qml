// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15
import QtQuick.Controls 2.15
import cc.etke.komai 1.0

TextArea {
    id: control

    readonly property color inputBackgroundColor: readOnly ? palette.alternateBase : palette.base

    font.pointSize: Settings.uiFontSizePt
    color: enabled ? palette.text : palette.buttonText
    placeholderTextColor: palette.buttonText
    selectionColor: palette.highlight
    selectedTextColor: palette.highlightedText
    selectByMouse: true
    padding: Komai.paddingSmall
    leftPadding: Komai.paddingMedium
    rightPadding: Komai.paddingMedium
    topPadding: Math.max(2, Komai.paddingSmall)
    bottomPadding: Math.max(2, Komai.paddingSmall)

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
