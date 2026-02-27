// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../"
import "../../components"
import "../../ui"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import im.nheko

ItemDelegate {
    id: roomItem

    required property bool compactMode
    required property int avatarSize
    required property bool collapsed
    readonly property real baseFontPixelSize: Qt.application.font.pixelSize > 0 ? Qt.application.font.pixelSize : 14
    required property var roomContextMenu
    required property var scrollbar
    required property string avatarUrl
    property color backgroundColor: palette.window
    property color bubbleBackground: palette.highlight
    property color bubbleText: palette.highlightedText
    required property string directChatOtherUserId
    required property bool hasLoudNotification
    required property bool hasUnreadMessages
    property color importantText: palette.text
    required property bool isDirect
    required property bool isInvite
    required property bool isSpace
    required property string lastMessage
    required property int notificationCount
    required property string roomId
    required property string roomName
    required property var tags
    required property string time
    required property bool isEncrypted
    property color unimportantText: palette.buttonText
    ToolTip.delay: Nheko.tooltipDelay
    ToolTip.text: roomName
    ToolTip.visible: hovered && collapsed
    height: Nheko.navigationRowHeight
    state: "normal"
    width: ListView.view.width - ((scrollbar && scrollbar.interactive && scrollbar.visible && scrollbar.parent) ? scrollbar.width : 0)

    topInset: 0
    bottomInset: 0
    leftInset: 0
    rightInset: 0

    background: Rectangle {
        color: backgroundColor

        Rectangle {
            anchors.fill: parent
            color: Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, 0.15)
            visible: hasUnreadMessages && roomItem.state !== "selected"
        }
    }
    states: [
        State {
            name: "highlight"
            when: roomItem.hovered && !((Rooms.currentRoom && roomId == Rooms.currentRoom.roomId) || Rooms.currentRoomPreview.roomid == roomId)

            PropertyChanges {
                roomItem {
                    backgroundColor: palette.dark
                    bubbleBackground: palette.highlight
                    bubbleText: palette.highlightedText
                    importantText: palette.brightText
                    unimportantText: palette.brightText
                }
            }
        },
        State {
            name: "selected"
            when: (Rooms.currentRoom && roomId == Rooms.currentRoom.roomId) || Rooms.currentRoomPreview.roomid == roomId

            PropertyChanges {
                roomItem {
                    backgroundColor: palette.highlight
                    bubbleBackground: palette.highlightedText
                    bubbleText: palette.highlight
                    importantText: palette.highlightedText
                    unimportantText: palette.highlightedText
                }
            }
        }
    ]

    onClicked: {
        console.log("tapped " + roomId);
        if (!Rooms.currentRoom || Rooms.currentRoom.roomId !== roomId)
            Rooms.setCurrentRoom(roomId);
        else
            Rooms.resetCurrentRoom();
    }
    onPressAndHold: {
        if (!isInvite)
            roomContextMenu.show(roomItem, roomId, tags);
    }

    Ripple {
        color: Qt.rgba(palette.dark.r, palette.dark.g, palette.dark.b, 0.5)
    }

    // NOTE(Nico): We want to prevent the touch areas from overlapping. For some reason we need to add 1px of padding for that...
    Item {
        anchors.fill: parent
        anchors.margins: 1

        TapHandler {
            id: roomItemTh

            acceptedButtons: Qt.RightButton
            acceptedDevices: PointerDevice.Mouse | PointerDevice.Stylus | PointerDevice.TouchPad
            gesturePolicy: TapHandler.ReleaseWithinBounds

            onSingleTapped: {
                if (!TimelineManager.isInvite)
                    roomContextMenu.show(roomItemTh.parent, roomId, tags);
            }
        }
    }
    RowLayout {
        anchors.fill: parent
        anchors.margins: Nheko.paddingMedium
        spacing: Nheko.paddingMedium

        Avatar {
            id: avatar

            Layout.alignment: Qt.AlignVCenter
            displayName: roomName
            enabled: false
            roomid: roomId
            url: avatarUrl.replace("mxc://", "image://MxcImage/")
            userid: isDirect ? directChatOtherUserId : ""
            Layout.preferredWidth: avatarSize
            Layout.preferredHeight: avatarSize

            NotificationBubble {
                id: collapsedNotificationBubble

                anchors.bottom: parent.bottom
                anchors.margins: -Nheko.paddingSmall
                anchors.right: parent.right
                bubbleBackgroundColor: roomItem.bubbleBackground
                bubbleTextColor: roomItem.bubbleText
                hasLoudNotification: roomItem.hasLoudNotification
                mayBeVisible: collapsed && (isSpace ? Settings.sidebarsRoomListShowCommunityCounts : true)
                notificationCount: roomItem.notificationCount
            }
        }
        ColumnLayout {
            id: textContent

            Layout.alignment: compactMode ? Qt.AlignVCenter : Qt.AlignLeft
            Layout.minimumWidth: 100
            Layout.preferredWidth: roomItem.width - avatar.width
            Layout.preferredHeight: compactMode ? -1 : avatar.height
            spacing: compactMode ? 0 : Nheko.paddingSmall
            visible: !collapsed

            Item {
                id: titleRow

                property bool previewsEnabled: !isSpace && (Settings.sidebarsRoomListLastMessagePreview === Settings.LastMessagePreview.Always || (Settings.sidebarsRoomListLastMessagePreview === Settings.LastMessagePreview.OnlyUnencrypted && !isEncrypted))

                Layout.alignment: Qt.AlignTop
                Layout.fillWidth: true
                Layout.preferredHeight: compactMode ? titleText.implicitHeight : subtitleText.implicitHeight

                ElidedLabel {
                    id: titleText

                    anchors.left: parent.left
                    anchors.verticalCenter: compactMode ? parent.verticalCenter : undefined
                    color: roomItem.importantText
                    elideWidth: parent.width - (timestamp.visible ? timestamp.implicitWidth + Nheko.paddingSmall : 0) - (spaceNotificationBubble.visible ? spaceNotificationBubble.implicitWidth + Nheko.paddingSmall : 0) - (inlinePreview.visible ? Nheko.paddingSmall : 0)
                    font.bold: hasUnreadMessages
                    fullText: TimelineManager.htmlEscape(roomName)
                    textFormat: Text.RichText
                }
                ElidedLabel {
                    id: inlinePreview

                    anchors.left: titleText.right
                    anchors.leftMargin: Nheko.paddingSmall
                    anchors.baseline: titleText.baseline
                    anchors.right: timestamp.visible ? timestamp.left : (spaceNotificationBubble.visible ? spaceNotificationBubble.left : parent.right)
                    anchors.rightMargin: (timestamp.visible || spaceNotificationBubble.visible) ? Nheko.paddingSmall : 0
                    color: roomItem.unimportantText
                    elideWidth: Math.max(0, parent.width - titleText.implicitWidth - Nheko.paddingSmall - (timestamp.visible ? timestamp.implicitWidth + Nheko.paddingSmall : (spaceNotificationBubble.visible ? spaceNotificationBubble.implicitWidth + Nheko.paddingSmall : 0)))
                    font.pixelSize: baseFontPixelSize * 0.95
                    fullText: TimelineManager.htmlEscape(lastMessage)
                    textFormat: Text.RichText
                    visible: compactMode && titleRow.previewsEnabled
                }
                Label {
                    id: timestamp

                    anchors.baseline: titleText.baseline
                    anchors.right: parent.right
                    color: roomItem.unimportantText
                    font.pixelSize: baseFontPixelSize * 0.95
                    text: time
                    visible: !isInvite && !isSpace && Nheko.sidebarsRoomListShowLastMessageTime
                }
                NotificationBubble {
                    id: spaceNotificationBubble

                    anchors.right: parent.right
                    bubbleBackgroundColor: roomItem.bubbleBackground
                    bubbleTextColor: roomItem.bubbleText
                    hasLoudNotification: roomItem.hasLoudNotification
                    mayBeVisible: !collapsed && (isSpace ? Settings.sidebarsRoomListShowCommunityCounts : compactMode)
                    notificationCount: roomItem.notificationCount
                    parent: (isSpace || compactMode) ? titleRow : subtextRow
                }
            }
            Item {
                id: subtextRow

                Layout.alignment: Qt.AlignBottom
                Layout.fillWidth: true
                Layout.preferredHeight: subtitleText.implicitHeight
                visible: !compactMode && !isSpace && (Settings.sidebarsRoomListLastMessagePreview === Settings.LastMessagePreview.Always || (Settings.sidebarsRoomListLastMessagePreview === Settings.LastMessagePreview.OnlyUnencrypted && !isEncrypted))

                ElidedLabel {
                    id: subtitleText

                    anchors.left: parent.left
                    color: roomItem.unimportantText
                    elideWidth: subtextRow.width - (subtextNotificationBubble.visible ? subtextNotificationBubble.implicitWidth : 0)
                    font.pixelSize: baseFontPixelSize * 0.95
                    fullText: TimelineManager.htmlEscape(lastMessage)
                    textFormat: Text.RichText
                }
                NotificationBubble {
                    id: subtextNotificationBubble

                    anchors.baseline: subtitleText.baseline
                    anchors.right: parent.right
                    bubbleBackgroundColor: roomItem.bubbleBackground
                    bubbleTextColor: roomItem.bubbleText
                    hasLoudNotification: roomItem.hasLoudNotification
                    mayBeVisible: !collapsed
                    notificationCount: roomItem.notificationCount
                }
            }
        }
    }
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        color: Nheko.theme.separator
        height: 1
    }
    Rectangle {
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        color: palette.highlight
        height: parent.height - Nheko.paddingSmall * 2
        visible: hasUnreadMessages
        width: 6
        radius: 3
    }
}
