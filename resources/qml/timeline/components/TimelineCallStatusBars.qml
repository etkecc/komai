// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../voip"
import QtQuick.Layouts

ColumnLayout {
    Layout.fillWidth: true
    spacing: 0

    CallInviteBar {
        Layout.fillWidth: true
        z: 3
    }

    ActiveCallBar {
        Layout.fillWidth: true
        z: 3
    }
}
