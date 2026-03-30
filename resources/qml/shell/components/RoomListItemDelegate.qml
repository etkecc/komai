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
    readonly property real baseFontPixelSize: Komai.fontPixelSize
    required property var roomContextMenu
    required property real scrollbarReservedWidth
    required property string avatarUrl
    property color backgroundColor: palette.window
    property color bubbleBackground: palette.highlight
    property color bubbleText: palette.highlightedText
    required property string directChatOtherUserId
    required property bool hasLoudNotification
    required property bool hasUnreadMessages
    required property bool hasDraft
    property color draftIndicatorColor: Komai.theme.attention
    property color importantText: palette.text
    required property bool isDirect
    required property bool isInvite
    required property bool isSpace
    required property string lastMessage
    required property string draftPreview
    required property int notificationCount
    required property string roomId
    required property string roomName
    required property var tags
    required property string time
    required property bool isEncrypted
    readonly property bool isSelected: roomId === Rooms.currentRoomId
    readonly property bool isLowPriorityRoom: !!tags && tags.indexOf && tags.indexOf("m.lowpriority") !== -1
    readonly property bool emphasizeUnreadState: hasUnreadMessages && (!isLowPriorityRoom || hasLoudNotification || Communities.currentFilterId === "tag:m.lowpriority")
    readonly property bool emphasizeDraftState: hasDraft && !emphasizeUnreadState
    readonly property bool emphasizeActivityState: emphasizeUnreadState || emphasizeDraftState
    readonly property bool keyboardFocused: ListView.view && ListView.view.activeFocus && ListView.isCurrentItem
    readonly property color draftActivityBase: Qt.rgba((Komai.theme.attention.r + palette.highlight.r) / 2, (Komai.theme.attention.g + palette.highlight.g) / 2, (Komai.theme.attention.b + palette.highlight.b) / 2, 1)
    readonly property color draftHoverBackground: Qt.rgba((palette.dark.r * 0.7) + (draftActivityBase.r * 0.3), (palette.dark.g * 0.7) + (draftActivityBase.g * 0.3), (palette.dark.b * 0.7) + (draftActivityBase.b * 0.3), 1)
    readonly property color draftSelectedBackground: Qt.rgba((palette.highlight.r * 0.75) + (draftActivityBase.r * 0.25), (palette.highlight.g * 0.75) + (draftActivityBase.g * 0.25), (palette.highlight.b * 0.75) + (draftActivityBase.b * 0.25), 1)
    property color unimportantText: palette.buttonText

    KomaiToolTip {
        anchorItem: roomItem
        anchorX: roomItem.width / 2
        anchorY: roomItem.height
        gapX: Komai.paddingMedium
        gapY: Komai.paddingMedium
        text: roomItem.roomName
        delay: Komai.tooltipDelay
        requestedVisible: roomItem.hovered && roomItem.collapsed
    }

    height: Komai.navigationRowHeight
    state: "normal"
    width: ListView.view.width - scrollbarReservedWidth

    topInset: 0
    bottomInset: 0
    leftInset: 0
    rightInset: 0
    activeFocusOnTab: false
    focusPolicy: Qt.NoFocus

    background: Rectangle {
        color: backgroundColor

        Rectangle {
            anchors.fill: parent
            color: roomItem.emphasizeDraftState
                ? Qt.rgba(Komai.theme.attention.r, Komai.theme.attention.g, Komai.theme.attention.b, 0.12)
                : Qt.rgba(palette.highlight.r, palette.highlight.g, palette.highlight.b, 0.15)
            visible: roomItem.emphasizeActivityState && roomItem.state !== "selected"
        }
        Rectangle {
            anchors.fill: parent
            color: "transparent"
            border.color: roomItem.isSelected ? palette.highlightedText : palette.highlight
            border.width: roomItem.keyboardFocused ? 2 : 0
        }
    }
    states: [
        State {
            name: "highlight"
            when: roomItem.hovered && !roomItem.isSelected

            PropertyChanges {
                roomItem {
                    backgroundColor: roomItem.emphasizeDraftState ? roomItem.draftHoverBackground : palette.dark
                    bubbleBackground: palette.highlight
                    bubbleText: palette.highlightedText
                    draftIndicatorColor: palette.brightText
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
                    backgroundColor: roomItem.emphasizeDraftState ? roomItem.draftSelectedBackground : palette.highlight
                    bubbleBackground: palette.highlightedText
                    bubbleText: palette.highlight
                    draftIndicatorColor: palette.highlightedText
                    importantText: palette.highlightedText
                    unimportantText: palette.highlightedText
                }
            }
        }
    ]

    onClicked: {
        console.log("tapped " + roomId);
        if (Rooms.currentRoomId !== roomId)
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

    KomaiCursorShape {
        anchors.fill: parent
        cursorShape: roomItem.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
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
        id: mainContent

        anchors.fill: parent
        anchors.leftMargin: Komai.paddingMedium + Komai.paddingSmall
        anchors.rightMargin: Komai.paddingMedium + Komai.paddingSmall
        anchors.topMargin: compactMode ? Komai.paddingSmall / 2 : Komai.paddingMedium
        anchors.bottomMargin: compactMode ? Komai.paddingSmall / 2 : Komai.paddingMedium
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
            hasDraft: roomItem.hasDraft
            hasLoudNotification: roomItem.hasLoudNotification
            notificationCount: roomItem.notificationCount
            avatarHeight: avatar.height
            baseFontPixelSize: roomItem.baseFontPixelSize
            roomName: roomItem.roomName
            lastMessage: roomItem.lastMessage
            draftPreview: roomItem.draftPreview
            time: roomItem.time
            draftIndicatorColor: roomItem.draftIndicatorColor
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
        anchors.leftMargin: Komai.paddingSmall / 2
        anchors.verticalCenter: parent.verticalCenter
        color: roomItem.emphasizeDraftState ? roomItem.draftIndicatorColor : palette.highlight
        height: parent.height - Komai.paddingMedium * 2
        visible: roomItem.emphasizeActivityState
        width: 6
        radius: 3
    }
}
