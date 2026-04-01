// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick.Layouts
import "../../components"

AvatarSettingsFlipButton {
    id: root

    required property var room
    required property string roomId
    required property string roomDisplayName
    required property string roomAvatarUrl
    required property bool isDirect
    required property string directChatOtherUserId
    required property int topBarAvatarSize
    required property int buttonPaddingH

    Layout.alignment: Qt.AlignVCenter
    Layout.column: 1
    Layout.rightMargin: buttonPaddingH
    Layout.preferredHeight: topBarAvatarSize
    Layout.preferredWidth: topBarAvatarSize
    Layout.row: 1
    avatarButtonSize: topBarAvatarSize
    avatarDisplayName: root.roomDisplayName
    avatarRoomId: roomId
    avatarUrl: roomAvatarUrl.replace("mxc://", "image://MxcImage/")
    avatarUserId: isDirect ? directChatOtherUserId : ""
    toolTipText: qsTr("Room settings")

    onLeftClicked: {
        if (root.room)
            TimelineManager.openRoomInfo(root.room.roomId, "settings");
    }
    onRightClicked: {
        if (root.room)
            TimelineManager.openRoomInfo(root.room.roomId, "settings");
    }
}
