// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import cc.etke.komai

Rectangle {
    id: root

    property color barColor: Qt.rgba(Qt.darker(palette.base, 2.1).r, Qt.darker(palette.base, 2.1).g, Qt.darker(palette.base, 2.1).b, 0.88)
    property real barRadius: Komai.paddingMedium
    property color barBorderColor: "transparent"
    property int barBorderWidth: 0

    color: barColor
    radius: barRadius
    border.color: barBorderColor
    border.width: barBorderWidth
}
