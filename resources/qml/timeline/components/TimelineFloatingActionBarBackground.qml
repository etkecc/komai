// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import cc.etke.komai

Rectangle {
    id: root

    property color barColor: Qt.rgba(Qt.darker(palette.base, 2.1).r, Qt.darker(palette.base, 2.1).g, Qt.darker(palette.base, 2.1).b, 0.88)

    color: barColor
    radius: Komai.paddingMedium
}
