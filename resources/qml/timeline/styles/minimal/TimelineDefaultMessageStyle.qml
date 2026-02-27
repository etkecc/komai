// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Window
import im.nheko

import "../.."

TimelineMessageStyleBase {
    id: wrapper
    // We return a larger size for any item but the most bottom one, if it isn't initialized yet, since otherwise Qt will create way too many items.
    // If we did that also for the first item, it would mess with the scroll location a bit, so we don't do it for that item.
    height: Math.max((section.item?.height ?? 0) + Math.max(((gridContainer.implicitHeight < 1 && index != 0) ? 100 : gridContainer.implicitHeight), (messageUserAvatar.visible ? messageUserAvatar.height : 0)) + reactionRow.implicitHeight + unreadRow.height, 10)
    //room: chatRoot.roommodel
    styleProfile: TimelineStyleProfile {
        fileMessagePadding: 12
        showFileMessageBackground: true
        showEncryptedMessageBackground: true
    }

    property int avatarMargin: (wrapper.isStateEvent ? 0 : (Nheko.avatarSize * (Settings.timelineMessagesLayoutSmallAvatars ? 0.5 : 1) + 8)) // align with avatar
    property bool avatarIsOnRight: wrapper.messageIsRightAligned
    property int messageContainerX: wrapper.avatarIsOnRight ? 0 : wrapper.avatarMargin
    property int threadInset: wrapper.threadId ? (4 + gridContainer.spacing) : 0

    property alias hovered: messageHover.hovered

    mainInset: (threadId ? (4 + Nheko.paddingSmall) : 0)
    replyInset: mainInset + 4 + Nheko.paddingSmall

    maxWidth: chat.delegateMaxWidth - avatarMargin - metadata.width
    hoverDismissTimerRef: hoverDismissTimer

    data: [
        Loader {
            id: section

            active: wrapper.startsNewMessageGroup
            //asynchronous: true
            sourceComponent: TimelineMinimalSectionHeader {
                day: wrapper.day
                isSender: wrapper.isSender
                isStateEvent: wrapper.isStateEvent
                parentWidth: wrapper.width
                previousMessageDay: wrapper.previousMessageDay
                previousMessageTimestamp: wrapper.previousMessageTimestamp
                previousMessageIsStateEvent: wrapper.previousMessageIsStateEvent
                previousMessageUserId: wrapper.previousMessageUserId
                timestamp: wrapper.timestamp
                userId: wrapper.userId
                userName: wrapper.userName
                userPowerlevel: wrapper.userPowerlevel
            }
            visible: status == Loader.Ready
            z: 4
        },
        Rectangle {
            // this looks better without margins
            anchors.fill: gridContainer
            radius: Nheko.paddingMedium
            color: (Settings.timelineMessagesHoverHighlight && messageHover.hovered) ? palette.alternateBase : "transparent"

            // This is partially duplicated by a later handler, however we need this to handle the remaining events around the reply.
            TapHandler {
                acceptedButtons: Qt.RightButton
                acceptedDevices: PointerDevice.Mouse | PointerDevice.Stylus | PointerDevice.TouchPad
                gesturePolicy: TapHandler.ReleaseWithinBounds

                onSingleTapped: (event) => {
                    wrapper.openMessageContextMenu(wrapper.main.hoveredLink, wrapper.main.copyText);
                    event.accepted = true;
                }
            }
        },
        Rectangle {
            id: scrollHighlight
            anchors.fill: gridContainer

            color: palette.highlight
            enabled: false
            opacity: 0
            visible: true
            z: 1

            states: State {
                name: "revealed"
                when: wrapper.scrolledToThis
            }
            transitions: Transition {
                from: ""
                to: "revealed"

                SequentialAnimation {
                    PropertyAnimation {
                        duration: 500
                        easing.type: Easing.InOutQuad
                        from: 0
                        properties: "opacity"
                        target: scrollHighlight
                        to: 1
                    }
                    PropertyAnimation {
                        duration: 500
                        easing.type: Easing.InOutQuad
                        from: 1
                        properties: "opacity"
                        target: scrollHighlight
                        to: 0
                    }
                    ScriptAction {
                        script: {
                            if (wrapper.room) {
                                wrapper.room.eventShown();
                            }
                        }
                    }
                }
            }
        },
        Avatar {
            id: messageUserAvatar

            ToolTip.delay: Nheko.tooltipDelay
            ToolTip.text: wrapper.userId
            ToolTip.visible: messageUserAvatar.hovered
            displayName: wrapper.userName
            height: Nheko.avatarSize * (Settings.timelineMessagesLayoutSmallAvatars ? 0.5 : 1)
            url: wrapper.avatarImageUrl(wrapper.userId)
            userid: wrapper.userId
            width: Nheko.avatarSize * (Settings.timelineMessagesLayoutSmallAvatars ? 0.5 : 1)

            visible: !wrapper.isStateEvent
            opacity: wrapper.startsNewMessageGroup ? 1.0 : 0.0

            x: wrapper.avatarIsOnRight ? (wrapper.width - width) : 0
            y: section.visible && section.active ? section.y + section.height : 0
            z: 5

            onClicked: {
                if (wrapper.room)
                    wrapper.room.openUserProfile(wrapper.userId)
            }

            Connections {
                function onRoomAvatarUrlChanged() {
                    messageUserAvatar.url = wrapper.avatarImageUrl(wrapper.userId);
                }
                target: wrapper.room
            }
        },
        Row {
            id: gridContainer

            width: wrapper.width - wrapper.avatarMargin
            x: wrapper.messageContainerX
            y: section.visible && section.active ? section.y + section.height : 0
            layoutDirection: wrapper.messageIsRightAligned ? Qt.RightToLeft : Qt.LeftToRight
            spacing: Nheko.paddingSmall

            HoverHandler {
                id: messageHover
                blocking: false
                onHoveredChanged: wrapper.handleMessageHoverChanged(hovered, wrapper.main)
            }

            Timer {
                id: hoverDismissTimer
                interval: 180
                repeat: false
                onTriggered: wrapper.handleHoverDismissTimerTriggered(messageHover.hovered)
            }

            AbstractButton {
                ToolTip.delay: Nheko.tooltipDelay
                ToolTip.text: qsTr("Part of a thread")
                ToolTip.visible: hovered
                height: contentColumn.height
                visible: wrapper.threadId
                width: 4

                onClicked: {
                    if (wrapper.room)
                        wrapper.room.thread = wrapper.threadId
                }

                Rectangle {
                    id: threadLine

                    anchors.fill: parent
                    color: TimelineManager.userColor(wrapper.threadId, palette.base)
                }
            }

            Item {
                id: stateEventSpacing
                visible: false
                width: 0
                height: 1
            }

            Column {
                id: contentColumn

                AbstractButton {
                    id: replyRow
                    visible: wrapper.replyTo

                    leftPadding: Nheko.paddingSmall + 4

                    property color userColor: wrapper.room
                        ? TimelineManager.roomUserColor(wrapper.room.roomId, wrapper.reply?.userId ?? '', palette.base, palette.highlight)
                        : TimelineManager.userColor(wrapper.reply?.userId ?? '', palette.base)

                    clip: true

                    NhekoCursorShape {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                    }

                    contentItem: Column {
                            spacing: 0

                            id: replyCol

                            AbstractButton {
                                id: replyUserButton

                                contentItem: Label {
                                    id: userName_
                                    text: wrapper.reply?.userName ?? 'missing name'
                                    color: Qt.darker(replyRow.userColor, 1.3)
                                    textFormat: Text.RichText
                                    width: wrapper.maxWidth
                                    //elideWidth: wrapper.maxWidth
                                }
                                onClicked: {
                                    if (wrapper.room)
                                        wrapper.room.openUserProfile(wrapper.reply?.userId)
                                }
                            }
                            data: [
                                replyUserButton,
                                wrapper.reply,
                            ]
                    }

                    background: Rectangle {
                        color: Qt.tint(palette.base, Qt.hsla(replyRow.userColor.hslHue, 0.5, replyRow.userColor.hslLightness, 0.1))
                        radius: Nheko.paddingMedium
                        clip: true

                        Rectangle {
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            anchors.left: parent.left

                            id: replyLine
                            color: replyRow.userColor
                            width: 4
                            radius: parent.radius
                        }
                    }

                    onClicked: {
                        let link = wrapper.reply.hoveredLink
                        if (link) {
                            Nheko.openLink(link)
                        } else {
                            console.log("Scrolling to "+wrapper.replyTo);
                            if (wrapper.room) {
                                wrapper.room.showEvent(wrapper.replyTo)
                            }
                        }
                    }
                    onPressAndHold: wrapper.openReplyContextMenu(wrapper.reply, wrapper.replyTo, pressX, pressY, replyLine.width, replyUserButton.implicitHeight)
                    TapHandler {
                        acceptedButtons: Qt.RightButton
                        onSingleTapped: (eventPoint) => wrapper.openReplyContextMenu(wrapper.reply, wrapper.replyTo, eventPoint.position.x, eventPoint.position.y, replyLine.width, replyUserButton.implicitHeight)
                        gesturePolicy: TapHandler.ReleaseWithinBounds
                        acceptedDevices: PointerDevice.Mouse | PointerDevice.Stylus | PointerDevice.TouchPad
                    }
                }

                data: [
                    replyRow, wrapper.main,
                ]
            }

            DragHandler {
                id: replyDragHandler
                enabled: Settings.uiInputTouchSwipeGesturesEnabled
                yAxis.enabled: false
                xAxis.enabled: true
                xAxis.minimum: wrapper.messageContainerX - 100
                xAxis.maximum: wrapper.messageContainerX
                onActiveChanged: {
                    if (!replyDragHandler.active) {
                        if (replyDragHandler.xAxis.minimum <= replyDragHandler.xAxis.activeValue + 1) {
                            if (wrapper.room)
                                wrapper.room.reply = wrapper.eventId
                        }
                        gridContainer.x = Qt.binding(function () {
                            return wrapper.messageContainerX;
                        });
                    }
                }
            }

            TapHandler {
                onDoubleTapped: {
                    if (wrapper.room)
                        wrapper.room.reply = wrapper.eventId
                }
            }
        },
        Rectangle {
            anchors.top: gridContainer.top
            x: gridContainer.x + contentColumn.x - 2 - (wrapper.messageIsRightAligned ? 0 : wrapper.threadInset)
            y: gridContainer.y - 2
            color: "transparent"
            border.color: Nheko.theme.red
            border.width: wrapper.notificationlevel == MtxEvent.Highlight ? 1 : 0
            radius: 8
            height: contentColumn.implicitHeight + 4
            width: contentColumn.implicitWidth + 4 + wrapper.threadInset
        },
            TimelineMetadata {
                id: metadata

                scaling: 1
                buttonScale: 1.5
                leadingActionInTrailingLayout: Settings.timelineMessagesPositioning === Settings.TimelineMessagesPositioning.AllRight

                anchors.left: undefined
                anchors.right: undefined
                x: wrapper.messageIsRightAligned ? 0 : (wrapper.width - width)
                y: section.visible && section.active ? section.y + section.height : 0

                visible: !wrapper.isStateEvent

                eventId: wrapper.eventId
                forceTrailingTimestampLayout: true
                status: wrapper.status
                trustlevel: wrapper.trustlevel
                isEdited: wrapper.isEdited
                isEncrypted: wrapper.isEncrypted
                threadId: wrapper.threadId
                timestamp: wrapper.timestamp
                room: wrapper.room
                isSender: wrapper.isSender
                actionBarActive: messageActions.pinned && messageActions.attached === wrapper
            },
        Connections {
            target: metadata
            function onActionToggled() {
                wrapper.togglePinnedMessageActions(metadata.actionToggleButton);
            }
        },
        Item {
            // We need this item to grab events, that otherwise would go to the TextArea in the main item. If we don't have this, it would trigger a right click menu on KDE...
            // https://invent.kde.org/frameworks/qqc2-desktop-style/-/blob/9d71fe874186009f76d392e203d9fa25a49f8be7/org.kde.desktop/TextArea.qml#L55

            anchors.fill: gridContainer
            anchors.topMargin: replyRow.height
            TapHandler {

                acceptedButtons: Qt.RightButton
                acceptedDevices: PointerDevice.Mouse | PointerDevice.Stylus | PointerDevice.TouchPad
                gesturePolicy: TapHandler.ReleaseWithinBounds

                onSingleTapped: (event) => {
                    wrapper.openMessageContextMenu(wrapper.main.hoveredLink, wrapper.main.copyText);
                }
            }
        },
        Reactions {
            id: reactionRow

            eventId: wrapper.eventId
            reactions: wrapper.reactions
            layoutDirection: wrapper.messageIsRightAligned ? Qt.RightToLeft : Qt.LeftToRight
            width: wrapper.width - wrapper.avatarMargin
            x: wrapper.messageContainerX

            anchors {
                top: gridContainer.bottom
                topMargin: 0
            }
        },
        Item {
            id: unreadRow

            height: visible ? (3 + Nheko.paddingSmall) : 0
            visible: wrapper.hasRoom && (wrapper.index > 0 && (wrapper.room.fullyReadEventId == wrapper.eventId))

            anchors {
                left: parent.left
                right: parent.right
                top: reactionRow.bottom
                topMargin: 5
            }

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                color: palette.highlight
                height: 3
            }
        }
    ]
}
