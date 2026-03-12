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
    readonly property var room: Rooms.getRoomById(roomId)
    readonly property bool isSpace: room && room.isSpace
    readonly property string roomName: room ? room.roomName : ""

    title: {
        if (roomName) {
            return isSpace
                ? qsTr("Leave the %1 space?").arg(roomName)
                : qsTr("Leave the %1 room?").arg(roomName);
        }
        return isSpace ? qsTr("Leave this space?") : qsTr("Leave this room?");
    }
    titleIcon: ":/icons/icons/ui/power-off.svg"

    Label {
        Layout.fillWidth: true
        color: palette.text
        wrapMode: Text.WordWrap
        text: qsTr("You will remain in any rooms you joined through it.")
        visible: leaveRoomRoot.isSpace
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Komai.paddingMedium

        Components.KomaiButton {
            text: qsTr("Cancel")
            onClicked: leaveRoomRoot.close()
        }

        Item {
            Layout.fillWidth: true
        }

        Components.KomaiButton {
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
}
