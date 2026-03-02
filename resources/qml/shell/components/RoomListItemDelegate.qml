// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "." as ShellComponents
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai

ItemDelegate {
    id: roomItem

    required property bool compactMode
    required property int avatarSize
    required property bool collapsed
    readonly property real baseFontPixelSize: Qt.application.font.pixelSize > 0 ? Qt.application.font.pixelSize : 14
    required property var roomContextMenu
    required property real scrollbarReservedWidth
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
    readonly property bool isSelected: (Rooms.currentRoom && roomId == Rooms.currentRoom.roomId) || Rooms.currentRoomPreview.roomid == roomId
    readonly property bool isLowPriorityRoom: !!tags && tags.indexOf && tags.indexOf("m.lowpriority") !== -1
    readonly property bool emphasizeUnreadState: hasUnreadMessages && (!isLowPriorityRoom || hasLoudNotification)
    property int hoverPrewarmDelayMs: 100
    property color unimportantText: palette.buttonText
    ToolTip.delay: Komai.tooltipDelay
    ToolTip.text: roomName
    ToolTip.visible: hovered && collapsed
    height: Komai.navigationRowHeight
    state: "normal"
    width: ListView.view.width - scrollbarReservedWidth

    topInset: 0
    bottomInset: 0
    leftInset: 0
    rightInset: 0

    background: Rectangle {
        color: backgroundColor

        Rectangle {
            anchors.fill: parent
            color: Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, 0.15)
            visible: roomItem.emphasizeUnreadState && roomItem.state !== "selected"
        }
    }
    states: [
        State {
            name: "highlight"
            when: roomItem.hovered && !roomItem.isSelected

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
            when: roomItem.isSelected

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

    Timer {
        id: hoverPrewarmTimer

        interval: roomItem.hoverPrewarmDelayMs
        repeat: false
        onTriggered: Rooms.prewarmRoom(roomId, "hover")
    }

    onHoveredChanged: {
        if (hovered && !isInvite && !isSelected) {
            Rooms.scheduleRoomPrewarm(roomId, "hover");
            hoverPrewarmTimer.restart();
        } else if (hoverPrewarmTimer.running) {
            hoverPrewarmTimer.stop();
            Rooms.cancelRoomPrewarm(roomId, "hover", "hover_lost");
        }
    }
    onClicked: {
        if (hoverPrewarmTimer.running) {
            hoverPrewarmTimer.stop();
            Rooms.cancelRoomPrewarm(roomId, "hover", "clicked");
        }
        console.log("tapped " + roomId);
        if (!Rooms.currentRoom || Rooms.currentRoom.roomId !== roomId)
            Rooms.setCurrentRoom(roomId);
        else
            Rooms.resetCurrentRoom();
    }
    onPressAndHold: {
        if (hoverPrewarmTimer.running) {
            hoverPrewarmTimer.stop();
            Rooms.cancelRoomPrewarm(roomId, "hover", "press_and_hold");
        }
        if (!isInvite)
            roomContextMenu.show(roomItem, roomId, tags);
    }
    Component.onDestruction: {
        if (hoverPrewarmTimer.running) {
            hoverPrewarmTimer.stop();
            Rooms.cancelRoomPrewarm(roomId, "hover", "delegate_destroyed");
        }
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
        anchors.margins: Komai.paddingMedium
        spacing: Komai.paddingMedium

        ShellComponents.RoomListItemAvatar {
            id: avatar

            avatarSize: roomItem.avatarSize
            roomName: roomItem.roomName
            roomId: roomItem.roomId
            avatarUrl: roomItem.avatarUrl
            isDirect: roomItem.isDirect
            directChatOtherUserId: roomItem.directChatOtherUserId
            bubbleBackground: roomItem.bubbleBackground
            bubbleText: roomItem.bubbleText
            hasLoudNotification: roomItem.hasLoudNotification
            collapsed: roomItem.collapsed
            isSpace: roomItem.isSpace
            notificationCount: roomItem.notificationCount
        }
        ShellComponents.RoomListItemTextContent {
            compactMode: roomItem.compactMode
            collapsed: roomItem.collapsed
            isSpace: roomItem.isSpace
            isInvite: roomItem.isInvite
            isEncrypted: roomItem.isEncrypted
            hasUnreadMessages: roomItem.emphasizeUnreadState
            hasLoudNotification: roomItem.hasLoudNotification
            notificationCount: roomItem.notificationCount
            avatarHeight: avatar.height
            baseFontPixelSize: roomItem.baseFontPixelSize
            roomName: roomItem.roomName
            lastMessage: roomItem.lastMessage
            time: roomItem.time
            importantText: roomItem.importantText
            unimportantText: roomItem.unimportantText
            bubbleBackground: roomItem.bubbleBackground
            bubbleText: roomItem.bubbleText
        }
    }
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        color: Komai.theme.separator
        height: 1
    }
    Rectangle {
        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        color: palette.highlight
        height: parent.height - Komai.paddingSmall * 2
        visible: roomItem.emphasizeUnreadState
        width: 6
        radius: 3
    }
}
