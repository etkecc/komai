// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components"
import QtQuick
import QtQuick.Layouts
import im.nheko

Avatar {
    id: root

    required property int avatarSize
    required property string roomName
    required property string roomId
    required property string avatarUrl
    required property bool isDirect
    required property string directChatOtherUserId
    required property color bubbleBackground
    required property color bubbleText
    required property bool hasLoudNotification
    required property bool collapsed
    required property bool isSpace
    required property int notificationCount

    Layout.alignment: Qt.AlignVCenter
    displayName: roomName
    enabled: false
    roomid: roomId
    url: avatarUrl.replace("mxc://", "image://MxcImage/")
    userid: isDirect ? directChatOtherUserId : ""
    Layout.preferredWidth: avatarSize
    Layout.preferredHeight: avatarSize

    NotificationBubble {
        anchors.bottom: parent.bottom
        anchors.margins: -Nheko.paddingSmall
        anchors.right: parent.right
        bubbleBackgroundColor: root.bubbleBackground
        bubbleTextColor: root.bubbleText
        hasLoudNotification: root.hasLoudNotification
        mayBeVisible: root.collapsed && (root.isSpace ? Settings.sidebarsRoomListShowCommunityCounts : true)
        notificationCount: root.notificationCount
    }
}
