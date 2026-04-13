// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai

ColumnLayout {
    id: root

    required property bool compactMode
    required property bool collapsed
    required property bool isSpace
    required property bool isInvite
    required property bool isEncrypted
    required property bool hasUnreadMessages
    required property bool hasLoudNotification
    required property bool hasDraft
    required property int notificationCount
    required property real avatarHeight
    required property real baseFontPixelSize
    required property string roomName
    required property string lastMessage
    required property string draftPreview
    required property string time
    required property color importantText
    required property color unimportantText
    required property color bubbleBackground
    required property color bubbleText
    required property color draftIndicatorColor

    Layout.alignment: compactMode ? Qt.AlignVCenter : Qt.AlignLeft
    Layout.fillWidth: true
    Layout.minimumWidth: 100
    Layout.preferredHeight: compactMode ? -1 : avatarHeight
    spacing: compactMode ? 0 : Komai.paddingSmall
    visible: !collapsed

    Item {
        id: titleRow

        property bool previewsEnabled: !root.isSpace && (Settings.navigationRoomListLastMessagePreview === Settings.LastMessagePreview.Always || (Settings.navigationRoomListLastMessagePreview === Settings.LastMessagePreview.OnlyUnencrypted && !root.isEncrypted))

        Layout.alignment: Qt.AlignTop
        Layout.fillWidth: true
        Layout.preferredHeight: titleText.implicitHeight

        ElidedLabel {
            id: titleText

            anchors.left: parent.left
            anchors.verticalCenter: root.compactMode ? parent.verticalCenter : undefined
            color: root.importantText
            elideWidth: parent.width - (inviteIcon.visible ? inviteIcon.width + Komai.paddingSmall : 0) - (timestamp.visible ? timestamp.implicitWidth + Komai.paddingSmall : 0) - (spaceNotificationBubble.visible ? spaceNotificationBubble.implicitWidth + Komai.paddingSmall : 0) - ((inlinePreview.visible || inlineDraftPreview.visible) ? Komai.paddingSmall : 0)
            font.bold: root.hasUnreadMessages
            font.pointSize: Settings.uiFontSizePt
            font.family: Komai.fontFamily
            fullText: TimelineManager.htmlEscape(root.roomName)
            textFormat: Text.RichText
        }
        Image {
            id: inviteIcon

            anchors.left: titleText.right
            anchors.leftMargin: Komai.paddingSmall
            anchors.verticalCenter: titleText.verticalCenter
            height: Math.round(Komai.fontPixelSize * 0.9)
            source: "image://colorimage/:/icons/icons/ui/state-member-change.svg?" + root.importantText
            sourceSize.height: height
            sourceSize.width: width
            visible: root.isInvite
            width: height
        }
        ElidedLabel {
            id: inlinePreview

            anchors.left: inviteIcon.visible ? inviteIcon.right : titleText.right
            anchors.leftMargin: Komai.paddingSmall
            anchors.baseline: titleText.baseline
            anchors.right: timestamp.visible ? timestamp.left : (spaceNotificationBubble.visible ? spaceNotificationBubble.left : parent.right)
            anchors.rightMargin: (timestamp.visible || spaceNotificationBubble.visible) ? Komai.paddingSmall : 0
            color: root.unimportantText
            elideWidth: Math.max(0, parent.width - titleText.implicitWidth - Komai.paddingSmall - (timestamp.visible ? timestamp.implicitWidth + Komai.paddingSmall : (spaceNotificationBubble.visible ? spaceNotificationBubble.implicitWidth + Komai.paddingSmall : 0)))
            font.pointSize: Settings.uiFontSizePt * 0.95
            fullText: root.lastMessage
            visible: false
        }
        Item {
            id: inlineDraftPreview

            anchors.left: inviteIcon.visible ? inviteIcon.right : titleText.right
            anchors.leftMargin: Komai.paddingSmall
            anchors.right: timestamp.visible ? timestamp.left : (spaceNotificationBubble.visible ? spaceNotificationBubble.left : parent.right)
            anchors.rightMargin: (timestamp.visible || spaceNotificationBubble.visible) ? Komai.paddingSmall : 0
            anchors.verticalCenter: titleText.verticalCenter
            clip: true
            height: inlineDraftText.implicitHeight
            visible: false

            Label {
                id: inlineDraftPrefix

                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                color: root.draftIndicatorColor
                font.pointSize: Settings.uiFontSizePt * 0.95
                text: qsTr("You:")
            }
            Image {
                id: inlineDraftIcon

                anchors.left: inlineDraftPrefix.right
                anchors.leftMargin: Komai.paddingSmall
                anchors.verticalCenter: parent.verticalCenter
                height: Math.round(Komai.fontPixelSize * 0.9)
                source: "image://colorimage/:/icons/icons/ui/edit.svg?" + root.draftIndicatorColor
                sourceSize.height: height
                sourceSize.width: width
                width: height
            }
            ElidedLabel {
                id: inlineDraftText

                anchors.left: inlineDraftIcon.right
                anchors.leftMargin: Komai.paddingSmall
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                color: root.draftIndicatorColor
                elideWidth: Math.max(0, parent.width - inlineDraftPrefix.implicitWidth - inlineDraftIcon.width - Komai.paddingSmall * 2)
                font.pointSize: Settings.uiFontSizePt * 0.95
                fullText: root.draftPreview
            }
        }
        Label {
            id: timestamp

            anchors.baseline: titleText.baseline
            anchors.right: parent.right
            color: root.unimportantText
            font.pointSize: Settings.uiFontSizePt * 0.95
            text: root.time
            visible: !root.isInvite && !root.isSpace && Komai.navigationRoomListShowLastMessageTime
        }
        NotificationBubble {
            id: spaceNotificationBubble

            anchors.right: parent.right
            bubbleBackgroundColor: root.bubbleBackground
            bubbleTextColor: root.bubbleText
            hasLoudNotification: root.hasLoudNotification
            mayBeVisible: !root.collapsed && (root.isSpace ? Settings.navigationRoomListShowCommunityCounts : !subtextRow.visible)
            notificationCount: root.notificationCount
            parent: (root.isSpace || !subtextRow.visible) ? titleRow : subtextRow
        }
    }
    Item {
        id: subtextRow

        Layout.alignment: Qt.AlignBottom
        Layout.fillWidth: true
        Layout.preferredHeight: root.hasDraft ? subtextDraftText.implicitHeight : subtitleText.implicitHeight
        visible: titleRow.previewsEnabled || root.isInvite

        ElidedLabel {
            id: subtitleText

            anchors.left: parent.left
            anchors.right: subtextNotificationBubble.visible ? subtextNotificationBubble.left : parent.right
            anchors.rightMargin: subtextNotificationBubble.visible ? Komai.paddingSmall : 0
            color: root.unimportantText
            elideWidth: subtextRow.width - (subtextNotificationBubble.visible ? subtextNotificationBubble.implicitWidth + Komai.paddingSmall : 0)
            font.pointSize: Settings.uiFontSizePt * 0.95
            fullText: root.lastMessage
            visible: !root.hasDraft
        }
        Item {
            id: subtextDraftPreview

            anchors.left: parent.left
            anchors.right: parent.right
            height: subtextDraftText.implicitHeight
            visible: root.hasDraft

            Label {
                id: subtextDraftPrefix

                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                color: root.draftIndicatorColor
                font.pointSize: Settings.uiFontSizePt * 0.95
                text: qsTr("You:")
            }
            Image {
                id: subtextDraftIcon

                anchors.left: subtextDraftPrefix.right
                anchors.leftMargin: Komai.paddingSmall
                anchors.verticalCenter: parent.verticalCenter
                height: Math.round(Komai.fontPixelSize * 0.9)
                source: "image://colorimage/:/icons/icons/ui/edit.svg?" + root.draftIndicatorColor
                sourceSize.height: height
                sourceSize.width: width
                width: height
            }
            ElidedLabel {
                id: subtextDraftText

                anchors.left: subtextDraftIcon.right
                anchors.leftMargin: Komai.paddingSmall
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                color: root.draftIndicatorColor
                elideWidth: Math.max(0, parent.width - (subtextNotificationBubble.visible ? subtextNotificationBubble.implicitWidth : 0) - subtextDraftPrefix.implicitWidth - subtextDraftIcon.width - Komai.paddingSmall * 2)
                font.pointSize: Settings.uiFontSizePt * 0.95
                fullText: root.draftPreview
            }
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
