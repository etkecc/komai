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

    required property int density
    required property int avatarSize
    required property bool collapsed
    readonly property real baseFontPixelSize: Komai.fontPixelSize
    required property var roomContextMenu
    required property real scrollbarReservedWidth
    required property var tabController
    required property string avatarUrl
    property color backgroundColor: palette.window
    property color bubbleBackground: palette.highlight
    property color bubbleText: palette.highlightedText
    required property string directChatOtherUserId
    required property bool hasLoudNotification
    required property bool hasUnreadMessages
    required property bool hasDraft
    property color draftIndicatorColor: Komai.theme.attention
    property color unreadIndicatorColor: palette.highlight
    property color importantText: palette.text
    required property bool isDirect
    required property bool isInvite
    required property bool isSpace
    required property string lastMessage
    required property string draftPreview
    required property int unreadCount
    required property string roomId
    required property string roomName
    required property var tags
    required property string time
    required property bool isEncrypted
    readonly property bool isSelected: roomId === Rooms.currentRoomId
    readonly property bool isLowPriorityRoom: !!tags && tags.indexOf && tags.indexOf("m.lowpriority") !== -1
    // emphasizeUnreadState governs every visual signal tied to unread state
    // (bold title, avatar bounce, row highlight, left-edge marker, bubble),
    // so the global toggle lives here; drafts stay emphasized regardless.
    readonly property bool emphasizeUnreadState: hasUnreadMessages && (!isLowPriorityRoom || hasLoudNotification || Communities.currentFilterId === "tag:m.lowpriority") && Settings.navigationRoomListShowUnreadIndicators
    readonly property bool emphasizeDraftState: hasDraft && !emphasizeUnreadState
    readonly property bool emphasizeActivityState: emphasizeUnreadState || emphasizeDraftState
    readonly property bool keyboardFocused: ListView.view && ListView.view.activeFocus && ListView.isCurrentItem
    readonly property color draftActivityBase: Qt.rgba((Komai.theme.attention.r + palette.highlight.r) / 2, (Komai.theme.attention.g + palette.highlight.g) / 2, (Komai.theme.attention.b + palette.highlight.b) / 2, 1)
    readonly property color hoverBackground: Qt.rgba(palette.dark.r * 0.30 + palette.window.r * 0.70, palette.dark.g * 0.30 + palette.window.g * 0.70, palette.dark.b * 0.30 + palette.window.b * 0.70, 1)
    readonly property color selectedBackground: Qt.rgba(palette.dark.r * 0.85 + palette.window.r * 0.15, palette.dark.g * 0.85 + palette.window.g * 0.15, palette.dark.b * 0.85 + palette.window.b * 0.15, 1)
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
            border.color: palette.highlight
            border.width: roomItem.keyboardFocused ? 2 : 0
        }
    }
    states: [
        State {
            name: "highlight"
            when: roomItem.hovered && !roomItem.isSelected

            PropertyChanges {
                roomItem {
                    backgroundColor: roomItem.hoverBackground
                    bubbleBackground: palette.highlight
                    bubbleText: palette.highlightedText
                    importantText: palette.text
                    unimportantText: palette.text
                }
            }
        },
        State {
            name: "selected"
            when: roomItem.isSelected

            PropertyChanges {
                roomItem {
                    backgroundColor: roomItem.selectedBackground
                    bubbleBackground: palette.highlight
                    bubbleText: palette.highlightedText
                    draftIndicatorColor: palette.brightText
                    importantText: palette.brightText
                    unimportantText: palette.brightText
                    unreadIndicatorColor: palette.brightText
                }
            }
        }
    ]

    onClicked: {} // Click logic handled by roomClickArea below for modifier detection.
    onPressAndHold: {
        if (!isInvite)
            roomContextMenu.show(roomItem, roomId, tags);
    }

    Ripple {
        color: Qt.rgba(palette.dark.r, palette.dark.g, palette.dark.b, 0.5)
    }

    // Keep 1px of padding here so the touch areas do not overlap.
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
        MouseArea {
            id: roomClickArea

            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.MiddleButton
            hoverEnabled: true
            cursorShape: roomItem.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor

            onClicked: function(mouse) {
                console.log("tapped " + roomId);
                if (mouse.button === Qt.MiddleButton) {
                    tabController.openTab(roomId);
                    return;
                }
                var ctrlHeld = !!(mouse.modifiers & Qt.ControlModifier);
                tabController.handleRoomClick(roomId, isInvite, ctrlHeld);
            }
            onPressAndHold: function(mouse) {
                if (!isInvite)
                    roomContextMenu.show(roomItem, roomId, tags);
            }
        }
    }
    RowLayout {
        id: mainContent

        anchors.fill: parent
        anchors.leftMargin: Komai.paddingMedium + Komai.paddingSmall
        anchors.rightMargin: Komai.paddingMedium + Komai.paddingSmall
        anchors.topMargin: density === Settings.Density.Spacious ? Komai.paddingMedium : Komai.paddingSmall / 2
        anchors.bottomMargin: density === Settings.Density.Spacious ? Komai.paddingMedium : Komai.paddingSmall / 2
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
            hasUnreadMessages: roomItem.emphasizeUnreadState
            collapsed: roomItem.collapsed
            isSpace: roomItem.isSpace
            unreadCount: roomItem.unreadCount
            bounceOnUnread: roomItem.emphasizeUnreadState
            isSelected: roomItem.isSelected
        }
        ShellComponents.RoomListItemTextContent {
            density: roomItem.density
            collapsed: roomItem.collapsed
            isSpace: roomItem.isSpace
            isInvite: roomItem.isInvite
            isEncrypted: roomItem.isEncrypted
            hasUnreadMessages: roomItem.emphasizeUnreadState
            hasDraft: roomItem.hasDraft
            hasLoudNotification: roomItem.hasLoudNotification
            unreadCount: roomItem.unreadCount
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
        color: roomItem.emphasizeDraftState ? Komai.theme.attention : roomItem.unreadIndicatorColor
        height: parent.height - Komai.paddingMedium * 2
        visible: roomItem.emphasizeActivityState
        width: 6
        radius: 3
    }
}
