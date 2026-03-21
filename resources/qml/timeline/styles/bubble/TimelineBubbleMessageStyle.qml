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
    // Events that slipped into the message index (e.g. encrypted events whose keys
    // arrived late and turned out to be reactions, edits, etc.) must be collapsed.
    visible: !isHiddenEvent
    height: isHiddenEvent ? 0 : Math.max((section.item?.height ?? 0) + Math.max(((gridContainer.implicitHeight < 1 && index != 0) ? 100 : gridContainer.implicitHeight), (reserveAvatarRowHeight && messageUserAvatar.visible ? messageUserAvatar.height : 0)) + reactionRow.implicitHeight + unreadRow.height, 10)
    //room: chatRoot.roommodel
    styleProfile: TimelineStyleProfile {
        fileMessagePadding: wrapper.styleFileMessagePadding
        showFileMessageBackground: wrapper.styleShowFileMessageBackground
        showEncryptedMessageBackground: wrapper.styleShowEncryptedMessageBackground
    }

    property int styleFileMessagePadding: 8
    property bool styleShowFileMessageBackground: false
    property bool styleShowEncryptedMessageBackground: false

    property int messageBubblePadding: Komai.uiLayoutCompactMode ? Komai.paddingSmall : Komai.paddingMedium
    property int messageBubbleHorizontalPadding: messageBubblePadding
    property int messageBubbleVerticalPadding: messageBubblePadding
    property int messageBubbleRadius: 8
    property bool messageBubbleBackgroundEnabled: true
    property bool alignMessageTextToSide: false
    property bool reserveAvatarRowHeight: startsNewMessageGroup
    property bool pushMetadataToEdge: false
    property bool alignBubbleToTop: true

    property bool shouldShowMessageAvatar: !wrapper.isStateEvent && (!wrapper.isSender || Settings.timelineMessagesLayoutShowOwnAvatar)
    property int avatarMargin: (shouldShowMessageAvatar ? (Komai.listIconSize * (Settings.timelineMessagesLayoutSmallAvatars ? 0.5 : 1) + 8) : 0) // align with avatar
    property bool avatarIsOnRight: wrapper.messageIsRightAligned

    property alias hovered: messageHover.hovered
    keyboardActionAnchorItem: messageBubble
    property real selectionTintOpacity: messageBubbleBackgroundEnabled ? 0.16 : 0.22
    readonly property color selectionOutlineColor: Qt.rgba(palette.highlight.r,
                                                            palette.highlight.g,
                                                            palette.highlight.b,
                                                            0.95)
    readonly property color focusedOutlineColor: Qt.rgba(selectionOutlineColor.r,
                                                         selectionOutlineColor.g,
                                                         selectionOutlineColor.b,
                                                         0.72)
    readonly property color selectedBorderColor: Qt.rgba(selectionOutlineColor.r,
                                                         selectionOutlineColor.g,
                                                         selectionOutlineColor.b,
                                                         1.0)
    readonly property color selectionTintColor: Qt.rgba(selectionOutlineColor.r,
                                                        selectionOutlineColor.g,
                                                        selectionOutlineColor.b,
                                                        selectionTintOpacity)
    mainMessageTextColor: (messageBubble && messageBubble.roomBubblePalette && messageBubble.roomBubblePalette.text !== undefined)
                          ? messageBubble.roomBubblePalette.text
                          : palette.text
    mainMessageSecondaryTextColor: (messageBubble && messageBubble.roomBubblePalette && messageBubble.roomBubblePalette.buttonText !== undefined)
                                   ? messageBubble.roomBubblePalette.buttonText
                                   : palette.buttonText
    mainMessageLinkColor: (messageBubble && messageBubble.roomBubblePalette && messageBubble.roomBubblePalette.link !== undefined)
                          ? messageBubble.roomBubblePalette.link
                          : palette.link
    mainMessageSurfaceColor: (messageBubble && messageBubble.roomBubblePalette && messageBubble.roomBubblePalette.alternateBase !== undefined)
                             ? messageBubble.roomBubblePalette.alternateBase
                             : palette.alternateBase
    replyMessageTextColor: (replyRow && replyRow.replyBubblePalette && replyRow.replyBubblePalette.text !== undefined)
                           ? replyRow.replyBubblePalette.text
                           : palette.text
    replyMessageSecondaryTextColor: (replyRow && replyRow.replyBubblePalette && replyRow.replyBubblePalette.buttonText !== undefined)
                                    ? replyRow.replyBubblePalette.buttonText
                                    : palette.buttonText
    replyMessageLinkColor: (replyRow && replyRow.replyBubblePalette && replyRow.replyBubblePalette.link !== undefined)
                           ? replyRow.replyBubblePalette.link
                           : palette.link
    replyMessageSurfaceColor: (replyRow && replyRow.replyBubblePalette && replyRow.replyBubblePalette.alternateBase !== undefined)
                              ? replyRow.replyBubblePalette.alternateBase
                              : palette.alternateBase

    mainInset: threadId ? (4 + Komai.paddingSmall) : 0
    replyInset: mainInset + 4 + Komai.paddingMedium + Komai.paddingMedium

    property int bubbleMargin: Math.max(metadataOuter.width + Komai.paddingSmall + (wrapper.isStateEvent ? 0 : 2 * messageBubbleHorizontalPadding), Math.round((chat.delegateMaxWidth - avatarMargin) * 0.15))

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
            id: threadBackground
            anchors.fill: gridContainer
            radius: 8
            property color threadColor: {
                const _revision = wrapper.timelineColorRevision;
                return TimelineManager.userColor(wrapper.threadId, palette.base);
            }
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
            id: selectionTint
            anchors.fill: gridContainer
            radius: 8
            color: wrapper.selectionTintColor
            visible: wrapper.selectedInView
            z: 0.5
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

            property int avatarSide: Math.round(Komai.listIconSize * (Settings.timelineMessagesLayoutSmallAvatars ? 0.5 : 1))

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
                y: wrapper.isStateEvent
                    ? Math.round((parent.height - height) / 2)
                    : (wrapper.alignBubbleToTop ? 0 : (parent.height - height))

                property color roomColor: wrapper.resolveUserColor(wrapper.userId, wrapper.themeBaseColor)
                property var roomBubblePalette: wrapper.resolveUserBubblePalette(wrapper.userId, roomColor)

                palette.window: roomBubblePalette.window
                palette.windowText: roomBubblePalette.windowText
                palette.base: roomBubblePalette.base
                palette.alternateBase: roomBubblePalette.alternateBase
                palette.text: roomBubblePalette.text
                palette.brightText: roomBubblePalette.brightText
                palette.button: roomBubblePalette.button
                palette.buttonText: roomBubblePalette.buttonText
                palette.light: roomBubblePalette.light
                palette.mid: roomBubblePalette.mid
                palette.dark: roomBubblePalette.dark
                palette.highlight: roomBubblePalette.highlight
                palette.highlightedText: roomBubblePalette.highlightedText
                palette.link: roomBubblePalette.link
                palette.toolTipBase: roomBubblePalette.toolTipBase
                palette.toolTipText: roomBubblePalette.toolTipText
                palette.inactive.text: roomBubblePalette.buttonText
                palette.inactive.windowText: roomBubblePalette.buttonText
                palette.inactive.buttonText: roomBubblePalette.buttonText

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
                            property color userColor: wrapper.resolveUserColor(replyUserId, wrapper.themeWindowColor)
                            property color roomColor: wrapper.resolveUserColor(replyUserId, wrapper.themeBaseColor)
                            property var replyBubblePalette: wrapper.resolveUserBubblePalette(replyUserId, roomColor)

                            palette.window: replyBubblePalette.window
                            palette.windowText: replyBubblePalette.windowText
                            palette.base: replyBubblePalette.base
                            palette.alternateBase: replyBubblePalette.alternateBase
                            palette.text: replyBubblePalette.text
                            palette.brightText: replyBubblePalette.brightText
                            palette.button: replyBubblePalette.button
                            palette.buttonText: replyBubblePalette.buttonText
                            palette.light: replyBubblePalette.light
                            palette.mid: replyBubblePalette.mid
                            palette.dark: replyBubblePalette.dark
                            palette.highlight: replyBubblePalette.highlight
                            palette.highlightedText: replyBubblePalette.highlightedText
                            palette.link: replyBubblePalette.link
                            palette.toolTipBase: replyBubblePalette.toolTipBase
                            palette.toolTipText: replyBubblePalette.toolTipText
                            palette.inactive.text: replyBubblePalette.buttonText
                            palette.inactive.windowText: replyBubblePalette.buttonText
                            palette.inactive.buttonText: replyBubblePalette.buttonText

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
                                            color: Komai.readableAccentTextColor(replyRow.userColor, replyRow.roomColor)
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
                                color: replyRow.roomColor
                                radius: Komai.paddingMedium
                                clip: true
                                Rectangle {
                                    anchors.top: parent.top
                                    anchors.bottom: parent.bottom
                                    anchors.left: parent.left

                                    id: replyLine
                                    color: replyRow.roomColor
                                    width: 4
                                }
                            }

                            // Border overlay drawn on top of content so rounded
                            // corners are not hidden by the content item.
                            Rectangle {
                                anchors.fill: parent
                                z: 10
                                color: "transparent"
                                radius: Komai.paddingMedium
                                border.width: 1
                                border.color: Qt.darker(replyRow.roomColor, 1.3)
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
                                    GradientStop { position: 1.0; color: replyRow.palette.base }
                                }
                            }
                        }

                        data: [replyRow, wrapper.main]
                    }


                }

                Binding {
                    target: wrapper.main
                    property: "palette.window"
                    when: !!wrapper.main
                    value: messageBubble.palette.window
                }

                Binding {
                    target: wrapper.main
                    property: "palette.windowText"
                    when: !!wrapper.main
                    value: messageBubble.palette.windowText
                }

                Binding {
                    target: wrapper.main
                    property: "palette.base"
                    when: !!wrapper.main
                    value: messageBubble.palette.base
                }

                Binding {
                    target: wrapper.main
                    property: "palette.alternateBase"
                    when: !!wrapper.main
                    value: messageBubble.palette.alternateBase
                }

                Binding {
                    target: wrapper.main
                    property: "palette.text"
                    when: !!wrapper.main
                    value: messageBubble.palette.text
                }

                Binding {
                    target: wrapper.main
                    property: "palette.brightText"
                    when: !!wrapper.main
                    value: messageBubble.palette.brightText
                }

                Binding {
                    target: wrapper.main
                    property: "palette.buttonText"
                    when: !!wrapper.main
                    value: messageBubble.palette.buttonText
                }

                Binding {
                    target: wrapper.main
                    property: "palette.dark"
                    when: !!wrapper.main
                    value: messageBubble.palette.dark
                }

                Binding {
                    target: wrapper.main
                    property: "palette.highlight"
                    when: !!wrapper.main
                    value: messageBubble.palette.highlight
                }

                Binding {
                    target: wrapper.main
                    property: "palette.highlightedText"
                    when: !!wrapper.main
                    value: messageBubble.palette.highlightedText
                }

                Binding {
                    target: wrapper.main
                    property: "palette.link"
                    when: !!wrapper.main
                    value: messageBubble.palette.link
                }

                Binding {
                    target: wrapper.main
                    property: "palette.inactive.text"
                    when: !!wrapper.main
                    value: messageBubble.palette.buttonText
                }

                Binding {
                    target: wrapper.main
                    property: "palette.inactive.windowText"
                    when: !!wrapper.main
                    value: messageBubble.palette.buttonText
                }

                Binding {
                    target: wrapper.main
                    property: "palette.inactive.buttonText"
                    when: !!wrapper.main
                    value: messageBubble.palette.buttonText
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
                        ? messageBubble.roomColor
                        : "transparent"
                    radius: wrapper.messageBubbleRadius
                    border.color: Komai.theme.attention
                    border.width: wrapper.notificationlevel == MtxEvent.Highlight ? 1 : 0

                    Canvas {
                        id: dashedBorderCanvas
                        anchors.fill: parent
                        visible: !wrapper.isStateEvent && wrapper.messageBubbleBackgroundEnabled && wrapper.status === MtxEvent.Sent

                        property real dashOffset
                        property color borderColor: messageBubble.roomColor

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
                anchors.verticalCenter: wrapper.isStateEvent ? gridContainer.verticalCenter : undefined

                anchors.left: undefined
                anchors.right: undefined
                x: {
                    if (wrapper.isStateEvent)
                        return Math.round(messageBubble.x + messageBubble.width + Komai.paddingSmall);
                    if (wrapper.pushMetadataToEdge) {
                        var threadInset = wrapper.threadId ? Komai.paddingSmall : 0;
                        return Math.round(wrapper.messageIsRightAligned
                            ? threadInset
                            : (gridContainer.width - width - threadInset));
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
        Canvas {
            id: selectionBorderCanvas
            anchors.fill: gridContainer
            z: 2.8
            visible: wrapper.selectedInView
            property color borderColor: wrapper.selectedBorderColor

            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            onBorderColorChanged: requestPaint()
            onVisibleChanged: requestPaint()

            onPaint: {
                var ctx = getContext("2d");
                ctx.clearRect(0, 0, width, height);
                ctx.strokeStyle = borderColor;
                ctx.lineWidth = 3;
                ctx.setLineDash([]);
                var r = 8;
                var inset = 1.5;
                ctx.beginPath();
                ctx.roundedRect(inset, inset, width - 2 * inset, height - 2 * inset, r, r);
                ctx.stroke();
            }
        },
        Canvas {
            id: focusedBorderCanvas
            anchors.fill: gridContainer
            z: 3.2
            visible: wrapper.focusedInView
            property color borderColor: wrapper.focusedOutlineColor

            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            onBorderColorChanged: requestPaint()
            onVisibleChanged: requestPaint()

            onPaint: {
                var ctx = getContext("2d");
                ctx.clearRect(0, 0, width, height);
                ctx.strokeStyle = borderColor;
                ctx.lineWidth = 1.5;
                ctx.setLineDash([]);
                var r = 8;
                var inset = 4;
                ctx.beginPath();
                ctx.roundedRect(inset, inset, width - 2 * inset, height - 2 * inset, r, r);
                ctx.stroke();
            }
        },
        Canvas {
            id: threadBorderCanvas
            anchors.fill: gridContainer
            z: 2
            visible: !!wrapper.threadId && !wrapper.isStateEvent

            property color borderColor: threadBackground.threadColor

            onWidthChanged: requestPaint()
            onHeightChanged: requestPaint()
            onBorderColorChanged: requestPaint()
            onVisibleChanged: requestPaint()

            onPaint: {
                var ctx = getContext("2d");
                ctx.clearRect(0, 0, width, height);
                ctx.strokeStyle = borderColor;
                ctx.lineWidth = 1.5;
                ctx.setLineDash([6, 10]);
                var r = 8;
                var inset = 0.75;
                ctx.beginPath();
                ctx.roundedRect(inset, inset, width - 2 * inset, height - 2 * inset, r, r);
                ctx.stroke();
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
