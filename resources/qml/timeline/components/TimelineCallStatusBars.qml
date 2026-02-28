// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../voip"
import QtQuick.Layouts

ColumnLayout {
    readonly property bool layoutVisible: callInviteBar.visible || activeCallBar.visible

    Layout.fillWidth: true
    Layout.minimumHeight: 0
    Layout.preferredHeight: layoutVisible ? implicitHeight : 0
    Layout.maximumHeight: layoutVisible ? implicitHeight : 0
    spacing: 0
    visible: layoutVisible

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
