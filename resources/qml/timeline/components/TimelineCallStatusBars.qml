// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../voip"
import QtQuick.Layouts
import im.nheko

ColumnLayout {
    readonly property bool showCallInviteBar: CallManager.haveCallInvite && Settings.uiInputMode && Settings.callsLegacyEnabled
    readonly property bool showActiveCallBar: CallManager.isOnCall && Settings.callsLegacyEnabled
    readonly property bool layoutVisible: showCallInviteBar || showActiveCallBar

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
