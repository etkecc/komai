// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../voip"
import QtQuick.Layouts

ColumnLayout {
    readonly property bool barsVisible: callInviteBar.visible || activeCallBar.visible

    Layout.fillWidth: true
    Layout.preferredHeight: barsVisible ? implicitHeight : 0
    spacing: 0
    visible: barsVisible

    CallInviteBar {
        id: callInviteBar

        Layout.fillWidth: true
        z: 3
    }

    ActiveCallBar {
        Layout.fillWidth: true
        z: 3
    }
}
