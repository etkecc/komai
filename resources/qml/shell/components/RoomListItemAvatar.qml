// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import cc.etke.komai

Avatar {
    id: root

    property int avatarSize: 0
    property string roomName: ""
    property string roomId: ""
    property string avatarUrl: ""
    property bool isDirect: false
    property string directChatOtherUserId: ""
    property color bubbleBackground: palette.highlight
    property color bubbleText: palette.highlightedText
    property bool hasLoudNotification: false
    property bool hasUnreadMessages: false
    property bool collapsed: false
    property bool isSpace: false
    property int unreadCount: 0
    property bool bounceOnUnread: false
    property bool isSelected: false

    Layout.alignment: Qt.AlignVCenter
    Layout.bottomMargin: avatarBounce.running ? -avatarBounce.offset : 0
    Layout.topMargin: avatarBounce.running ? avatarBounce.offset : 0
    displayName: roomName
    enabled: false
    roomid: roomId
    url: avatarUrl.replace("mxc://", "image://MxcImage/")
    userid: isDirect ? directChatOtherUserId : ""
    Layout.preferredWidth: avatarSize
    Layout.preferredHeight: avatarSize

    UnreadBounceAnimation {
        id: avatarBounce
        active: root.bounceOnUnread && !root.isSelected
    }

    NotificationBubble {
        anchors.bottom: parent.bottom
        anchors.margins: -Komai.paddingSmall
        anchors.right: parent.right
        bubbleBackgroundColor: root.bubbleBackground
        bubbleTextColor: root.bubbleText
        hasLoudNotification: root.hasLoudNotification
        mayBeVisible: root.collapsed && (root.isSpace ? Settings.navigationCommunitiesShowUnreadIndicators : root.hasUnreadMessages)
        unreadCount: root.unreadCount
    }
}
