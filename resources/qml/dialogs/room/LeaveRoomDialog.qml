// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components" as Components
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import cc.etke.komai 1.0

Components.OverlayDialog {
    id: leaveRoomRoot

    required property string roomId
    property string reason: ""

    title: qsTr("Leave room")
    titleIcon: ":/icons/icons/ui/power-off.svg"

    Label {
        Layout.fillWidth: true
        color: palette.text
        wrapMode: Text.WordWrap
        text: qsTr("Are you sure you want to leave?")
    }

    Button {
        Layout.alignment: Qt.AlignRight
        text: qsTr("Leave")
        highlighted: true
        onClicked: {
            if (CallManager.haveCallInvite) {
                CallManager.rejectInvite();
            } else if (CallManager.isOnCall) {
                CallManager.hangUp();
            }
            Rooms.leave(leaveRoomRoot.roomId, leaveRoomRoot.reason);
            leaveRoomRoot.close();
        }
    }
}
