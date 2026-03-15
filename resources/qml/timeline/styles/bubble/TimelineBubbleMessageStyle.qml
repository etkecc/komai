// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Window
import cc.etke.komai
import "../../../components"

TimelineMessageStyleBase {
    id: wrapper
    // We return a larger size for any item but the most bottom one, if it isn't initialized yet, since otherwise Qt will create way too many items.
    // If we did that also for the first item, it would mess with the scroll location a bit, so we don't do it for that item.
    height: Math.max((section.item?.height ?? 0) + Math.max(((gridContainer.implicitHeight < 1 && index != 0) ? 100 : gridContainer.implicitHeight), (reserveAvatarRowHeight && messageUserAvatar.visible ? messageUserAvatar.height : 0)) + reactionRow.implicitHeight + unreadRow.height, 10)
    //room: chatRoot.roommodel
    styleProfile: TimelineStyleProfile {
        fileMessagePadding: wrapper.styleFileMessagePadding
        showFileMessageBackground: wrapper.styleShowFileMessageBackground
        showEncryptedMessageBackground: wrapper.styleShowEncryptedMessageBackground
    }

    property int styleFileMessagePadding: 8
    property bool styleShowFileMessageBackground: false
    property bool styleShowEncryptedMessageBackground: false

    property int messageBubblePadding: Komai.paddingMedium
    property int messageBubbleHorizontalPadding: messageBubblePadding
    property int messageBubbleVerticalPadding: messageBubblePadding
    property int messageBubbleRadius: 8
    property bool messageBubbleBackgroundEnabled: true
    property bool alignMessageTextToSide: false
    property bool reserveAvatarRowHeight: false
    property bool pushMetadataToEdge: false

    property bool shouldShowMessageAvatar: !wrapper.isStateEvent && (!wrapper.isSender || Settings.timelineMessagesLayoutShowOwnAvatar)
    property int avatarMargin: (shouldShowMessageAvatar ? (Komai.avatarSize * (Settings.timelineMessagesLayoutSmallAvatars ? 0.5 : 1) + 8) : 0) // align with avatar
    property bool avatarIsOnRight: wrapper.messageIsRightAligned

    property alias hovered: messageHover.hovered

    mainInset: threadId ? (4 + Komai.paddingSmall) : 0
    replyInset: mainInset + 4 + Komai.paddingMedium + Komai.paddingMedium

    property int bubbleMargin: Math.max(metadataOuter.width + Komai.paddingMedium, Math.round((chat.delegateMaxWidth - avatarMargin) * 0.15))

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
                roomRef: wrapper.roomForColorCoding
                colorRoomId: wrapper.roomIdForColorCoding
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
        AvatarUserFlipButton {
            id: messageUserAvatar

            property int avatarSide: Math.round(Komai.avatarSize * (Settings.timelineMessagesLayoutSmallAvatars ? 0.5 : 1))

            avatarButtonSize: avatarSide
            cleanFront: true
            avatarDisplayName: wrapper.userName
            avatarUrl: wrapper.avatarImageUrl(wrapper.userId)
            avatarUserId: wrapper.userId
            avatarRoomId: wrapper.roomIdForColorCoding
            toolTipText: wrapper.userId
            width: avatarSide
            height: avatarSide

            visible: wrapper.shouldShowMessageAvatar
            opacity: wrapper.startsNewMessageGroup ? 1.0 : 0.0

            x: wrapper.avatarIsOnRight ? (wrapper.width - width) : 0
            y: (section.visible && section.active ? section.y + section.height : 0)
            z: 5

            onLeftClicked: {
                if (wrapper.room) {
                    wrapper.room.openUserProfile(wrapper.userId)
                }
            }

            Connections {
                function onRoomAvatarUrlChanged() {
                    messageUserAvatar.avatarUrl = wrapper.avatarImageUrl(wrapper.userId);
                }
                target: wrapper.room
            }
        },
        Item {
            id: gridContainer

            width: wrapper.width - wrapper.avatarMargin
            implicitHeight: Math.max(messageBubble.implicitHeight, metadataOuter.visible ? metadataOuter.height : 0)
            x: wrapper.avatarIsOnRight ? 0 : wrapper.avatarMargin
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

                anchors.horizontalCenter: undefined
                anchors.left: undefined
                anchors.right: undefined
                x: (wrapper.isStateEvent || !wrapper.messageIsRightAligned) ? 0 : (parent.width - width)
                anchors.bottom: parent.bottom

                property color roomColor: wrapper.resolveUserColor(wrapper.userId, palette.base)

                contentItem: Item {
                    id: contentPlacementContainer

                    // Avoid a width->implicitWidth feedback path when delegates reflow.
                    property real replyContentWidth: wrapper.reply?.implicitWidth ?? 0
                    property real mainContentWidth: wrapper.main?.implicitWidth ?? 0

                    // Cap implicit width to maxWidth so the bubble never overflows
                    // its container when litehtml content_width exceeds the render
                    // constraint (e.g. <pre> blocks with long lines).
                    implicitWidth: Math.min(
                        Math.max(replyContentWidth + wrapper.replyInset, mainContentWidth + wrapper.mainInset),
                        wrapper.maxWidth
                    )
                    implicitHeight: contentColumn.implicitHeight

                    Column {
                        id: contentColumn
                        spacing: Komai.paddingMedium

                        anchors.left: parent.left
                        anchors.right: parent.right

                        AbstractButton {
                            id: replyRow
                            visible: wrapper.replyTo

                            readonly property int maxReplyHeight: Math.max(80, Math.round(chat.height * 0.25))
                            readonly property real replyContentHeight: replyCol.implicitHeight + topPadding + bottomPadding
                            readonly property bool replyTruncated: replyContentHeight > maxReplyHeight

                            leftPadding: Komai.paddingMedium + 4
                            rightPadding: Komai.paddingMedium
                            topPadding: Komai.paddingMedium
                            bottomPadding: Komai.paddingMedium

                            anchors.left: parent.left
                            anchors.right: parent.right
                            height: replyTruncated ? maxReplyHeight : undefined

                            property string replyUserId: {
                                if (wrapper.room && wrapper.replyTo) {
                                    const modelUserId = wrapper.room.dataById(wrapper.replyTo, Room.UserId, wrapper.eventId);
                                    if (typeof modelUserId === "string" && modelUserId.length > 0)
                                        return modelUserId;
                                }

                                const delegateUserId = wrapper.reply?.userId;
                                return (typeof delegateUserId === "string") ? delegateUserId : "";
                            }
                            property bool isReplyFromCurrentUser: {
                                const currentUser = Komai.currentUser;
                                const currentUserId = (currentUser && currentUser.userid)
                                        ? String(currentUser.userid)
                                        : "";
                                return currentUserId.length > 0 && replyUserId === currentUserId;
                            }
                            property color userColor: isReplyFromCurrentUser
                                ? Komai.theme.userColorSelf
                                : wrapper.resolveUserColor(replyUserId, palette.window)
                            property color roomColor: isReplyFromCurrentUser
                                ? Komai.theme.userColorSelf
                                : wrapper.resolveUserColor(replyUserId, palette.base)

                            clip: true

                            KomaiCursorShape {
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
                                color: Qt.tint(palette.base, Qt.hsla(replyRow.roomColor.hslHue, 0.5, replyRow.roomColor.hslLightness, 0.1))
                                radius: Komai.paddingMedium
                                clip: true
                                Rectangle {
                                    anchors.top: parent.top
                                    anchors.bottom: parent.bottom
                                    anchors.left: parent.left

                                    id: replyLine
                                    color: replyRow.roomColor
                                    width: 4
                                    radius: parent.radius
                                }
                            }

                            onClicked: {
                                let link = wrapper.reply.hoveredLink
                                if (link) {
                                    Komai.openLink(link)
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

                            // Gradient fade when reply preview is truncated
                            Rectangle {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                height: 40
                                visible: replyRow.replyTruncated
                                gradient: Gradient {
                                    GradientStop { position: 0.0; color: "transparent" }
                                    GradientStop { position: 1.0; color: palette.base }
                                }
                            }
                        }

                        data: [replyRow, wrapper.main]
                    }


                }

                Binding {
                    // Plain style can align textual delegates with the active side.
                    target: wrapper.main
                    property: "horizontalAlignment"
                    when: wrapper.alignMessageTextToSide
                          && !wrapper.isStateEvent
                          && !!wrapper.main
                          && typeof wrapper.main.horizontalAlignment !== "undefined"
                    value: wrapper.messageIsRightAligned ? Text.AlignRight : Text.AlignLeft
                }

                Binding {
                    target: wrapper.main
                    property: "timelineViewportHeight"
                    when: !!wrapper.main && typeof wrapper.main.timelineViewportHeight !== "undefined"
                    value: chat.height
                }

                leftPadding: wrapper.isStateEvent ? 0 : wrapper.messageBubbleHorizontalPadding
                rightPadding: wrapper.isStateEvent ? 0 : wrapper.messageBubbleHorizontalPadding
                topPadding: wrapper.isStateEvent ? 0 : wrapper.messageBubbleVerticalPadding
                bottomPadding: wrapper.isStateEvent ? 0 : wrapper.messageBubbleVerticalPadding
                background: Rectangle {
                    color: (!wrapper.isStateEvent && wrapper.messageBubbleBackgroundEnabled)
                        ? (wrapper.isSender
                            ? Qt.tint(palette.base, Qt.hsla(Komai.theme.userColorSelf.hslHue, wrapper.hovered ? 0.8 : 0.6, Komai.theme.userColorSelf.hslLightness, 0.3))
                            : Qt.tint(palette.base, Qt.hsla(messageBubble.roomColor.hslHue, wrapper.hovered ? 0.8 : 0.5, messageBubble.roomColor.hslLightness, 0.2)))
                        : "transparent"
                    radius: wrapper.messageBubbleRadius
                    border.color: Komai.theme.attention
                    border.width: wrapper.notificationlevel == MtxEvent.Highlight ? 1 : 0

                    Canvas {
                        id: dashedBorderCanvas
                        anchors.fill: parent
                        visible: !wrapper.isStateEvent && wrapper.messageBubbleBackgroundEnabled && wrapper.status === MtxEvent.Sent

                        property real dashOffset
                        property color borderColor: wrapper.isSender
                            ? Qt.hsla(Komai.theme.userColorSelf.hslHue, 0.7, Komai.theme.userColorSelf.hslLightness, 0.6)
                            : Qt.hsla(messageBubble.roomColor.hslHue, 0.6, messageBubble.roomColor.hslLightness, 0.5)

                        NumberAnimation on dashOffset {
                            from: 0
                            to: 16
                            duration: 800
                            loops: Animation.Infinite
                            running: dashedBorderCanvas.visible
                        }

                        onDashOffsetChanged: requestPaint()
                        onWidthChanged: requestPaint()
                        onHeightChanged: requestPaint()
                        onBorderColorChanged: requestPaint()
                        onVisibleChanged: {
                            if (!visible)
                                dashOffset = 0;
                            requestPaint();
                        }

                        onPaint: {
                            var ctx = getContext("2d");
                            ctx.clearRect(0, 0, width, height);
                            ctx.strokeStyle = borderColor;
                            ctx.lineWidth = 1.5;
                            ctx.setLineDash([6, 10]);
                            ctx.lineDashOffset = -dashOffset;
                            var r = wrapper.messageBubbleRadius;
                            var inset = 0.75;
                            ctx.beginPath();
                            ctx.roundedRect(inset, inset, width - 2 * inset, height - 2 * inset, r, r);
                            ctx.stroke();
                        }
                    }
                }
            }

            TimelineMetadata {
                id: metadataOuter

                scaling: 0.9

                visible: !wrapper.isStateEvent
                    || Settings.timelineMessageActionsActivationPolicy === Settings.TimelineMessageActionsActivationPolicy.ActionsButton

                // Bottom-align with the bubble content area
                anchors.bottom: wrapper.isStateEvent ? undefined : messageBubble.bottom
                anchors.bottomMargin: wrapper.isStateEvent
                    ? 0
                    : Math.round(Math.max(1, messageBubble.bottomPadding - (metadataOuter.height - fontMetrics.height) / 2))
                anchors.top: wrapper.isStateEvent ? messageBubble.top : undefined
                // State events can include taller inline payloads (e.g. avatar previews).
                // Anchor metadata to the leading text line instead of the bottom edge.
                anchors.topMargin: wrapper.isStateEvent
                    ? Math.round((fontMetrics.height - metadataOuter.height) / 2)
                    : 0

                anchors.left: undefined
                anchors.right: undefined
                x: {
                    if (wrapper.isStateEvent)
                        return Math.round(messageBubble.x + messageBubble.width + Komai.paddingSmall);
                    if (wrapper.pushMetadataToEdge) {
                        return Math.round(wrapper.messageIsRightAligned
                            ? 0
                            : (gridContainer.width - width));
                    }
                    const sideX = wrapper.messageIsRightAligned
                        ? (messageBubble.x - width - Komai.paddingSmall)
                        : (messageBubble.x + messageBubble.width + Komai.paddingSmall);
                    return Math.round(sideX);
                }

                eventId: wrapper.eventId
                status: wrapper.status
                trustlevel: wrapper.trustlevel
                isEdited: wrapper.isEdited
                isEncrypted: wrapper.isEncrypted
                isStateEvent: wrapper.isStateEvent
                threadId: wrapper.threadId
                timestamp: wrapper.timestamp
                room: wrapper.room
                // Metadata order (timestamp/status/actions) should follow the active bubble side.
                isSender: wrapper.isStateEvent ? false : wrapper.messageIsRightAligned
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
                xAxis.minimum: (wrapper.messageIsRightAligned ? 0 : wrapper.avatarMargin) - 100
                xAxis.maximum: wrapper.messageIsRightAligned ? 0 : wrapper.avatarMargin
                onActiveChanged: {
                    if (!replyDragHandler.active) {
                        if (replyDragHandler.xAxis.minimum <= replyDragHandler.xAxis.activeValue + 1) {
                            if (wrapper.room) {
                                wrapper.room.reply = wrapper.eventId
                            }
                        }
                        gridContainer.x = Qt.binding(function () {
                            return wrapper.avatarIsOnRight ? 0 : wrapper.avatarMargin;
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
            layoutDirection: (!wrapper.isStateEvent && wrapper.messageIsRightAligned) ? Qt.RightToLeft : Qt.LeftToRight
            reactions: wrapper.reactions
            width: wrapper.width - wrapper.avatarMargin
            x: wrapper.avatarIsOnRight ? 0 : wrapper.avatarMargin

            anchors {
                //left: row.bubbleOnRight ? undefined : row.left
                //right: row.bubbleOnRight ? row.right : undefined
                top: gridContainer.bottom
                topMargin: 1
            }
        },
        Item {
            id: unreadRow

            height: visible ? (3 + Komai.paddingSmall) : 0
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
