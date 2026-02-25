// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Window
import im.nheko

import "./components"

TimelineEvent {
    id: wrapper
    ListView.delayRemove: true
    width: chat.delegateMaxWidth
    // We return a larger size for any item but the most bottom one, if it isn't initialized yet, since otherwise Qt will create way too many items.
    // If we did that also for the first item, it would mess with the scroll location a bit, so we don't do it for that item.
    height: Math.max((section.item?.height ?? 0) + Math.max(((gridContainer.implicitHeight < 1 && index != 0) ? 100 : gridContainer.implicitHeight), (messageUserAvatar.visible ? messageUserAvatar.height : 0)) + reactionRow.implicitHeight + unreadRow.height, 10)
    anchors.horizontalCenter: ListView.view.contentItem.horizontalCenter
    //room: chatRoot.roommodel

    required property var day
    required property bool isSender
    required property int index
    property var previousMessageDay: (index + 1) >= chat.count ? 0 : chat.model.dataByIndex(index + 1, Room.Day)
    property var previousMessageTimestamp: (index + 1) >= chat.count ? 0 : chat.model.dataByIndex(index + 1, Room.Timestamp)
    property bool previousMessageIsStateEvent: (index + 1) >= chat.count ? true : chat.model.dataByIndex(index + 1, Room.IsStateEvent)
    property string previousMessageUserId: (index + 1) >= chat.count ? "" : chat.model.dataByIndex(index + 1, Room.UserId)

    required property date timestamp
    required property string userId
    required property string userName
    required property string threadId
    required property int userPowerlevel
    required property bool isEdited
    required property bool isEncrypted
    required property var reactions
    required property int status
    required property int trustlevel
    required property int notificationlevel
    required property int type
    required property bool isEditable

    required property QtObject messageContextMenu
    required property QtObject replyContextMenu
    required property Item messageActions

    property int avatarMargin: (wrapper.isStateEvent ? 0 : (Nheko.avatarSize * (Settings.timelineMessagesLayoutSmallAvatars ? 0.5 : 1) + 8)) // align with avatar

    property alias hovered: messageHover.hovered

    property int oneHour: 60 * 60 * 1000
    property bool showSection: wrapper.previousMessageDay !== wrapper.day || wrapper.timestamp - wrapper.previousMessageTimestamp > oneHour
    readonly property bool hasRoom: wrapper.room !== null

    mainInset: (threadId ? (4 + Nheko.paddingSmall) : 0)
    replyInset: mainInset + 4 + Nheko.paddingSmall

    maxWidth: chat.delegateMaxWidth - avatarMargin - metadata.width

    function openMessageActions(pin, anchorItem) {
        if (!anchorItem)
            return;

        hoverDismissTimer.stop();
        messageActions.model = wrapper;
        messageActions.attached = wrapper;
        messageActions.pinned = pin;

        var pos = anchorItem.mapToItem(chat.contentItem, 0, 0);
        var barW = messageActions.implicitWidth;

        // Y: bar opens upward from anchor top
        messageActions.y = pos.y - messageActions.implicitHeight;

        var leftBound = wrapper.x + Nheko.paddingLarge;
        var rightBound = wrapper.x + wrapper.width - Nheko.paddingLarge;
        var minX = leftBound;
        var maxX = rightBound - barW;
        if (maxX < minX) {
            minX = wrapper.x;
            maxX = wrapper.x + wrapper.width - barW;
        }
        if (pin) {
            // X (button mode): center on anchor, clamped to delegate bounds
            var centerX = pos.x + anchorItem.width / 2 - barW / 2;
            messageActions.x = Math.max(minX, Math.min(centerX, maxX));
        } else {
            // X (hover mode): align to message side
            messageActions.x = wrapper.isSender ? maxX : minX;
        }
    }

    data: [
        Loader {
            id: section

            active: wrapper.previousMessageUserId !== wrapper.userId || wrapper.showSection || wrapper.previousMessageIsStateEvent !== wrapper.isStateEvent
            //asynchronous: true
            sourceComponent: TimelineSectionHeader {
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
                    messageContextMenu.show(wrapper.eventId, wrapper.threadId, wrapper.type, wrapper.isSender, wrapper.isEncrypted, wrapper.isEditable, wrapper.main.hoveredLink, wrapper.main.copyText);
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
            url: !wrapper.room ? "" : wrapper.room.avatarUrl(wrapper.userId).replace("mxc://", "image://MxcImage/")
            userid: wrapper.userId
            width: Nheko.avatarSize * (Settings.timelineMessagesLayoutSmallAvatars ? 0.5 : 1)

            visible: !wrapper.isStateEvent
            opacity: (wrapper.previousMessageUserId !== wrapper.userId || wrapper.showSection || wrapper.previousMessageIsStateEvent !== wrapper.isStateEvent) ? 1.0 : 0.0

            x: 0
            y: section.visible && section.active ? section.y + section.height : 0
            z: 5

            onClicked: {
                if (wrapper.room)
                    wrapper.room.openUserProfile(wrapper.userId)
            }

            Connections {
                function onRoomAvatarUrlChanged() {
                    if (wrapper.room) {
                        messageUserAvatar.url =
                          wrapper.room.avatarUrl(wrapper.userId).replace("mxc://", "image://MxcImage/");
                    }
                }
                target: wrapper.room
            }
        },
        Row {
            id: gridContainer

            width: wrapper.width - wrapper.avatarMargin
            x: wrapper.avatarMargin
            y: section.visible && section.active ? section.y + section.height : 0
            spacing: Nheko.paddingSmall

            HoverHandler {
                id: messageHover
                blocking: false
                onHoveredChanged: {
                    if (Settings.timelineMessageActionsActivationPolicy !== Settings.TimelineMessageActionsPolicy.OnHover)
                        return;

                    if (hovered) {
                        hoverDismissTimer.stop();
                        wrapper.openMessageActions(false, wrapper.main);
                    } else if (messageActions.attached === wrapper && !messageActions.pinned) {
                        hoverDismissTimer.restart();
                    }
                }
            }

            Timer {
                id: hoverDismissTimer
                interval: 180
                repeat: false
                onTriggered: {
                    if (Settings.timelineMessageActionsActivationPolicy !== Settings.TimelineMessageActionsPolicy.OnHover)
                        return;
                    if (messageActions.attached !== wrapper || messageActions.pinned)
                        return;
                    if (messageHover.hovered || messageActions.hovered)
                        return;
                    messageActions.dismiss();
                }
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

                    property color userColor: TimelineManager.roomUserColor(wrapper.room ? wrapper.room.roomId : '', wrapper.reply?.userId ?? '', palette.base, palette.highlight)

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
                    onPressAndHold: wrapper.replyContextMenu.show(wrapper.reply.copyText ?? "", wrapper.reply.linkAt ? wrapper.reply.linkAt(pressX-replyLine.width - Nheko.paddingSmall, pressY - replyUserButton.implicitHeight) : "", wrapper.replyTo)
                    TapHandler {
                        acceptedButtons: Qt.RightButton
                        onSingleTapped: (eventPoint) => wrapper.replyContextMenu.show(wrapper.reply.copyText ?? "", wrapper.reply.linkAt ? wrapper.reply.linkAt(eventPoint.position.x-replyLine.width - Nheko.paddingSmall, eventPoint.position.y - replyUserButton.implicitHeight) : "", wrapper.replyTo)
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
                xAxis.minimum: wrapper.avatarMargin - 100
                xAxis.maximum: wrapper.avatarMargin
                onActiveChanged: {
                    if (!replyDragHandler.active) {
                        if (replyDragHandler.xAxis.minimum <= replyDragHandler.xAxis.activeValue + 1) {
                            if (wrapper.room)
                                wrapper.room.reply = wrapper.eventId
                        }
                        gridContainer.x = wrapper.avatarMargin;
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
            anchors.left: gridContainer.left
            anchors.topMargin: -2
            anchors.leftMargin: -2 + (stateEventSpacing.visible ? (stateEventSpacing.width + gridContainer.spacing) : 0)
            color: "transparent"
            border.color: Nheko.theme.red
            border.width: wrapper.notificationlevel == MtxEvent.Highlight ? 1 : 0
            radius: 8
            height: contentColumn.implicitHeight + 4
            width: contentColumn.implicitWidth + 4 + (wrapper.threadId ? (4 + gridContainer.spacing) : 0)
        },
            TimelineMetadata {
                id: metadata

                scaling: 1
                buttonScale: 1.5

                anchors.right: parent.right
                y: section.visible && section.active ? section.y + section.height : 0

                visible: !wrapper.isStateEvent

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
            },
        Connections {
            target: metadata
            function onActionToggled() {
                if (messageActions.pinned && messageActions.attached === wrapper) {
                    messageActions.dismiss();
                } else {
                    wrapper.openMessageActions(true, metadata.actionToggleButton);
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
                    messageContextMenu.show(wrapper.eventId, wrapper.threadId, wrapper.type, wrapper.isSender, wrapper.isEncrypted, wrapper.isEditable, wrapper.main.hoveredLink, wrapper.main.copyText);
                }
            }
        },
        Reactions {
            id: reactionRow

            eventId: wrapper.eventId
            reactions: wrapper.reactions
            width: wrapper.width - wrapper.avatarMargin
            x: wrapper.avatarMargin

            anchors {
                top: gridContainer.bottom
                topMargin: -4
            }
        },
        Rectangle {
            id: unreadRow

            color: palette.highlight
            height: visible ? 3 : 0
            visible: wrapper.hasRoom && (wrapper.index > 0 && (wrapper.room.fullyReadEventId == wrapper.eventId))

            anchors {
                left: parent.left
                right: parent.right
                top: reactionRow.bottom
                topMargin: 5
            }
        }
    ]
}
