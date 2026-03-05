// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15
import QtQuick.Controls 2.15
import cc.etke.komai 1.0

ToolTip {
    id: control

    property color textColor
    property color backgroundColor

    font.pointSize: Settings.uiFontSizePt
    padding: Komai.paddingSmall
    leftPadding: Komai.paddingMedium
    rightPadding: Komai.paddingMedium

    contentItem: Text {
        text: control.text
        font: control.font
        color: control.textColor
        wrapMode: Text.WrapAnywhere
        width: control.availableWidth
    }

    background: Rectangle {
        color: control.backgroundColor
        radius: Komai.paddingSmall
        border.color: Komai.theme.separator
        border.width: 1
    }
}
