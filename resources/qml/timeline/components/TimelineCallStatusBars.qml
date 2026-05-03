// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../voip"
import QtQuick.Layouts
import cc.etke.komai

ColumnLayout {
    readonly property bool showActiveCallBar: CallManager.isOnCall
        && CallManager.preMatrixRtcCallsEnabled

    Layout.fillWidth: true
    Layout.minimumHeight: 0
    Layout.preferredHeight: showActiveCallBar ? implicitHeight : 0
    Layout.maximumHeight: showActiveCallBar ? implicitHeight : 0
    spacing: 0
    visible: showActiveCallBar

    ActiveCallBar {
        Layout.fillWidth: true
        z: 3
    }
}
