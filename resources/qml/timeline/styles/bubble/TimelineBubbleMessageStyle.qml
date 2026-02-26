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
    height: Math.max((section.item?.height ?? 0) + ((gridContainer.implicitHeight < 1 && index != 0) ? 100 : gridContainer.implicitHeight) + reactionRow.implicitHeight + unreadRow.height, 10)
    //room: chatRoot.roommodel
    styleProfile: TimelineStyleProfile {
        fileMessagePadding: 8
        showFileMessageBackground: false
        showEncryptedMessageBackground: false
    }

    property bool shouldShowMessageAvatar: !wrapper.isStateEvent && (!wrapper.isSender || Settings.timelineMessagesLayoutShowOwnAvatar)
    property int avatarMargin: (shouldShowMessageAvatar ? (Nheko.avatarSize * (Settings.timelineMessagesLayoutSmallAvatars ? 0.5 : 1) + 8) : 0) // align with avatar

    property alias hovered: messageHover.hovered

    mainInset: threadId ? (4 + Nheko.paddingSmall) : 0
    replyInset: mainInset + 4 + Nheko.paddingMedium + Nheko.paddingMedium

    property int bubbleMargin: Math.max(metadataOuter.width + Nheko.paddingMedium, Math.round((chat.delegateMaxWidth - avatarMargin) * 0.15))

    maxWidth: chat.delegateMaxWidth - avatarMargin - bubbleMargin
    hoverDismissTimerRef: hoverDismissTimer

    data: [
        Loader {
            id: section

            active: wrapper.startsNewMessageGroup
            //asynchronous: true
            sourceComponent: TimelineBubbleSectionHeader {
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
            anchors.fill: gridContainer
            radius: 8
            property color threadColor: TimelineManager.userColor(wrapper.threadId, palette.base)
            property color threadBackgroundColor: wrapper.threadId ? Qt.tint(palette.base, Qt.hsla(threadColor.hslHue, 0.7, threadColor.hslLightness, 0.1)) : "transparent"
            color: (Settings.timelineMessagesHoverHighlight && messageHover.hovered) ? palette.alternateBase : threadBackgroundColor

            // this looks better without margins
            TapHandler {
                acceptedButtons: Qt.RightButton
                acceptedDevices: PointerDevice.Mouse | PointerDevice.Stylus | PointerDevice.TouchPad
                gesturePolicy: TapHandler.ReleaseWithinBounds

                onSingleTapped: wrapper.openMessageContextMenu(wrapper.main.hoveredLink, wrapper.main.copyText)
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
                            if (wrapper.room)
                                wrapper.room.eventShown()
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

            visible: wrapper.shouldShowMessageAvatar
            opacity: wrapper.startsNewMessageGroup ? 1.0 : 0.0

            x: wrapper.isSender ? (wrapper.width - width) : 0
            y: (section.visible && section.active ? section.y + section.height : 0)
            z: 5

            onClicked: {
                if (wrapper.room) {
                    wrapper.room.openUserProfile(wrapper.userId)
                }
            }

            Connections {
                function onRoomAvatarUrlChanged() {
                    messageUserAvatar.url = wrapper.avatarImageUrl(wrapper.userId);
                }
                target: wrapper.room
            }
        },
        Item {
            id: gridContainer

            width: wrapper.width - wrapper.avatarMargin
            implicitHeight: messageBubble.implicitHeight
            x: wrapper.isSender ? 0 : wrapper.avatarMargin
            y: section.visible && section.active ? section.y + section.height : 0

            HoverHandler {
                id: messageHover
                blocking: false
                onHoveredChanged: wrapper.handleMessageHoverChanged(hovered, messageBubble)
            }

            Timer {
                id: hoverDismissTimer
                interval: 180
                repeat: false
                onTriggered: wrapper.handleHoverDismissTimerTriggered(messageHover.hovered)
            }


            AbstractButton {
                id: messageBubble

                anchors.left: (wrapper.isStateEvent || !wrapper.isSender) ? parent.left : undefined // qmllint disable Quick.anchor-combinations
                anchors.right: (!wrapper.isStateEvent && wrapper.isSender) ? parent.right : undefined
                anchors.horizontalCenter: undefined

                property color roomColor: wrapper.room
                    ? TimelineManager.roomUserColor(wrapper.room.roomId, wrapper.userId, palette.base, palette.highlight)
                    : TimelineManager.userColor(wrapper.userId, palette.base)

                contentItem: Item {
                    id: contentPlacementContainer

                    property real replyContentWidth: Math.max(wrapper.reply?.width ?? 0, wrapper.reply?.implicitWidth ?? 0)
                    property real mainContentWidth: Math.max(wrapper.main?.width ?? 0, wrapper.main?.implicitWidth ?? 0)

                    implicitWidth: Math.max(replyContentWidth + wrapper.replyInset, mainContentWidth + wrapper.mainInset)
                    implicitHeight: contentColumn.implicitHeight

                    Column {
                        id: contentColumn
                        spacing: Nheko.paddingMedium

                        anchors.left: parent.left
                        anchors.right: parent.right

                        AbstractButton {
                            id: replyRow
                            visible: wrapper.replyTo

                            leftPadding: Nheko.paddingMedium + 4
                            rightPadding: Nheko.paddingMedium
                            topPadding: Nheko.paddingMedium
                            bottomPadding: Nheko.paddingMedium

                            anchors.left: parent.left
                            anchors.right: parent.right

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
                                            if (wrapper.room) {
                                                wrapper.room.openUserProfile(wrapper.reply?.userId)
                                            }
                                        }
                                    }
                                    data: [
                                        replyUserButton,
                                        wrapper.reply,
                                    ]
                            }

                            background: Rectangle {
                                //width: replyRow.implicitContentWidth
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

                        data: [replyRow, wrapper.main]
                    }


                }

                padding: wrapper.isStateEvent ? 0 : Nheko.paddingMedium
                background: Rectangle {
                    color: !wrapper.isStateEvent ? (wrapper.isSender ? Qt.tint(palette.base, Qt.hsla(palette.highlight.hslHue, wrapper.hovered ? 0.8 : 0.6, palette.highlight.hslLightness, 0.3)) : Qt.tint(palette.base, Qt.hsla(messageBubble.roomColor.hslHue, wrapper.hovered ? 0.8 : 0.5, messageBubble.roomColor.hslLightness, 0.2))) : "transparent"
                    radius: 8
                    border.color: Nheko.theme.red
                    border.width: wrapper.notificationlevel == MtxEvent.Highlight ? 1 : 0
                }
            }

            TimelineMetadata {
                id: metadataOuter

                scaling: 0.9

                visible: !wrapper.isStateEvent

                // Bottom-align with the bubble content area
                anchors.bottom: messageBubble.bottom
                anchors.bottomMargin: messageBubble.padding - (metadataOuter.height - fontMetrics.height) / 2

                // Sender: metadata to the left of the bubble
                // Received: metadata to the right of the bubble
                anchors.right: wrapper.isSender ? messageBubble.left : undefined // qmllint disable Quick.anchor-combinations
                anchors.left: wrapper.isSender ? undefined : messageBubble.right
                anchors.rightMargin: wrapper.isSender ? Nheko.paddingSmall : 0
                anchors.leftMargin: wrapper.isSender ? 0 : Nheko.paddingSmall

                eventId: wrapper.eventId
                status: wrapper.status
                trustlevel: wrapper.trustlevel
                isEdited: wrapper.isEdited
                isEncrypted: wrapper.isEncrypted
                threadId: wrapper.threadId
                timestamp: wrapper.timestamp
                room: wrapper.room
                isSender: wrapper.isSender
                actionBarActive: messageActions.pinned && messageActions.attached === wrapper
            }

            Connections {
                target: metadataOuter
                function onActionToggled() {
                    wrapper.togglePinnedMessageActions(metadataOuter.actionToggleButton);
                }
            }

            DragHandler {
                id: replyDragHandler
                enabled: Settings.uiInputTouchSwipeGesturesEnabled
                yAxis.enabled: false
                xAxis.enabled: true
                xAxis.minimum: (wrapper.isSender ? 0 : wrapper.avatarMargin) - 100
                xAxis.maximum: wrapper.isSender ? 0 : wrapper.avatarMargin
                onActiveChanged: {
                    if (!replyDragHandler.active) {
                        if (replyDragHandler.xAxis.minimum <= replyDragHandler.xAxis.activeValue + 1) {
                            if (wrapper.room) {
                                wrapper.room.reply = wrapper.eventId
                            }
                        }
                        gridContainer.x = wrapper.isSender ? 0 : wrapper.avatarMargin;
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
            layoutDirection: (!wrapper.isStateEvent && wrapper.isSender) ? Qt.RightToLeft : Qt.LeftToRight
            reactions: wrapper.reactions
            width: wrapper.width - wrapper.avatarMargin
            x: wrapper.isSender ? 0 : wrapper.avatarMargin

            anchors {
                //left: row.bubbleOnRight ? undefined : row.left
                //right: row.bubbleOnRight ? row.right : undefined
                top: gridContainer.bottom
                topMargin: 1
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
