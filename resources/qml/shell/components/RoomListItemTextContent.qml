// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components"
import "../../ui"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import im.nheko

ColumnLayout {
    id: root

    required property bool compactMode
    required property bool collapsed
    required property bool isSpace
    required property bool isInvite
    required property bool isEncrypted
    required property bool hasUnreadMessages
    required property bool hasLoudNotification
    required property int notificationCount
    required property real avatarHeight
    required property real baseFontPixelSize
    required property string roomName
    required property string lastMessage
    required property string time
    required property color importantText
    required property color unimportantText
    required property color bubbleBackground
    required property color bubbleText

    Layout.alignment: compactMode ? Qt.AlignVCenter : Qt.AlignLeft
    Layout.fillWidth: true
    Layout.minimumWidth: 100
    Layout.preferredHeight: compactMode ? -1 : avatarHeight
    spacing: compactMode ? 0 : Nheko.paddingSmall
    visible: !collapsed

    Item {
        id: titleRow

        property bool previewsEnabled: !root.isSpace && (Settings.sidebarsRoomListLastMessagePreview === Settings.LastMessagePreview.Always || (Settings.sidebarsRoomListLastMessagePreview === Settings.LastMessagePreview.OnlyUnencrypted && !root.isEncrypted))

        Layout.alignment: Qt.AlignTop
        Layout.fillWidth: true
        Layout.preferredHeight: root.compactMode ? titleText.implicitHeight : subtitleText.implicitHeight

        ElidedLabel {
            id: titleText

            anchors.left: parent.left
            anchors.verticalCenter: root.compactMode ? parent.verticalCenter : undefined
            color: root.importantText
            elideWidth: parent.width - (timestamp.visible ? timestamp.implicitWidth + Nheko.paddingSmall : 0) - (spaceNotificationBubble.visible ? spaceNotificationBubble.implicitWidth + Nheko.paddingSmall : 0) - (inlinePreview.visible ? Nheko.paddingSmall : 0)
            font.bold: root.hasUnreadMessages
            fullText: TimelineManager.htmlEscape(root.roomName)
            textFormat: Text.RichText
        }
        ElidedLabel {
            id: inlinePreview

            anchors.left: titleText.right
            anchors.leftMargin: Nheko.paddingSmall
            anchors.baseline: titleText.baseline
            anchors.right: timestamp.visible ? timestamp.left : (spaceNotificationBubble.visible ? spaceNotificationBubble.left : parent.right)
            anchors.rightMargin: (timestamp.visible || spaceNotificationBubble.visible) ? Nheko.paddingSmall : 0
            color: root.unimportantText
            elideWidth: Math.max(0, parent.width - titleText.implicitWidth - Nheko.paddingSmall - (timestamp.visible ? timestamp.implicitWidth + Nheko.paddingSmall : (spaceNotificationBubble.visible ? spaceNotificationBubble.implicitWidth + Nheko.paddingSmall : 0)))
            font.pixelSize: root.baseFontPixelSize * 0.95
            fullText: TimelineManager.htmlEscape(root.lastMessage)
            textFormat: Text.RichText
            visible: root.compactMode && titleRow.previewsEnabled
        }
        Label {
            id: timestamp

            anchors.baseline: titleText.baseline
            anchors.right: parent.right
            color: root.unimportantText
            font.pixelSize: root.baseFontPixelSize * 0.95
            text: root.time
            visible: !root.isInvite && !root.isSpace && Nheko.sidebarsRoomListShowLastMessageTime
        }
        NotificationBubble {
            id: spaceNotificationBubble

            anchors.right: parent.right
            bubbleBackgroundColor: root.bubbleBackground
            bubbleTextColor: root.bubbleText
            hasLoudNotification: root.hasLoudNotification
            mayBeVisible: !root.collapsed && (root.isSpace ? Settings.sidebarsRoomListShowCommunityCounts : root.compactMode)
            notificationCount: root.notificationCount
            parent: (root.isSpace || root.compactMode) ? titleRow : subtextRow
        }
    }
    Item {
        id: subtextRow

        Layout.alignment: Qt.AlignBottom
        Layout.fillWidth: true
        Layout.preferredHeight: subtitleText.implicitHeight
        visible: !root.compactMode && !root.isSpace && (Settings.sidebarsRoomListLastMessagePreview === Settings.LastMessagePreview.Always || (Settings.sidebarsRoomListLastMessagePreview === Settings.LastMessagePreview.OnlyUnencrypted && !root.isEncrypted))

        ElidedLabel {
            id: subtitleText

            anchors.left: parent.left
            color: root.unimportantText
            elideWidth: subtextRow.width - (subtextNotificationBubble.visible ? subtextNotificationBubble.implicitWidth : 0)
            font.pixelSize: root.baseFontPixelSize * 0.95
            fullText: TimelineManager.htmlEscape(root.lastMessage)
            textFormat: Text.RichText
        }
        NotificationBubble {
            id: subtextNotificationBubble

            anchors.baseline: subtitleText.baseline
            anchors.right: parent.right
            bubbleBackgroundColor: root.bubbleBackground
            bubbleTextColor: root.bubbleText
            hasLoudNotification: root.hasLoudNotification
            mayBeVisible: !root.collapsed
            notificationCount: root.notificationCount
        }
    }
}
