// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components"
import QtQuick
import QtQuick.Layouts
import im.nheko

Avatar {
    id: root

    required property var room
    required property string roomId
    required property string avatarUrl
    required property bool isDirect
    required property string directChatOtherUserId
    required property int topBarAvatarSize
    required property int buttonPaddingH

    Layout.alignment: Qt.AlignVCenter
    Layout.column: 1
    Layout.rightMargin: buttonPaddingH
    Layout.row: 1
    displayName: room ? room.plainRoomName : qsTr("No room selected")
    enabled: false
    implicitHeight: topBarAvatarSize
    implicitWidth: topBarAvatarSize
    roomid: roomId
    url: avatarUrl.replace("mxc://", "image://MxcImage/")
    userid: isDirect ? directChatOtherUserId : ""

    MouseArea {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor

        onClicked: {
            if (root.room)
                TimelineManager.openRoomSettings(root.room.roomId);
        }
    }
}
