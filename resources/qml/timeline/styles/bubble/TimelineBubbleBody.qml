// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Window
import cc.etke.komai

Item {
    id: root

    required property var wrapper
    property real topOffset: 0
    property alias hovered: messageHover.hovered
    property alias hoverDismissTimer: hoverDismissTimer
    property alias bubbleItem: messageBubble
    property alias replyItem: replyRow
    property alias metadataItem: metadataOuter

    width: wrapper.width - wrapper.avatarMargin
    height: implicitHeight
    implicitHeight: Math.max(messageBubble.implicitHeight, metadataOuter.visible ? metadataOuter.height : 0)
    x: wrapper.avatarIsOnRight ? 0 : wrapper.avatarMargin
    y: topOffset

    Rectangle {
        id: threadBackground
        anchors.fill: parent
        radius: 8
        property color threadColor: {
            const _revision = root.wrapper.timelineColorRevision;
            return TimelineManager.userColor(root.wrapper.threadId, palette.base);
        }
        property color threadBackgroundColor: root.wrapper.threadId ? Qt.tint(palette.base, Qt.hsla(threadColor.hslHue, 0.7, threadColor.hslLightness, 0.1)) : "transparent"
        color: (Settings.timelineMessagesHoverHighlight && messageHover.hovered) ? palette.alternateBase : threadBackgroundColor

        TapHandler {
            acceptedButtons: Qt.RightButton
            acceptedDevices: PointerDevice.Mouse | PointerDevice.Stylus | PointerDevice.TouchPad
            gesturePolicy: TapHandler.ReleaseWithinBounds

            onSingleTapped: root.wrapper.openMessageContextMenu(root.wrapper.main.hoveredLink, root.wrapper.main.copyText)
        }
    }
    Rectangle {
        id: selectionTint
        anchors.fill: parent
        radius: 8
        color: root.wrapper.selectionTintColor
        visible: root.wrapper.selectedInView
        z: 0.5
    }
    Rectangle {
        id: scrollHighlight
        anchors.fill: parent

        color: palette.highlight
        enabled: false
        opacity: 0
        visible: true
        z: 1

        states: State {
            name: "revealed"
            when: root.wrapper.scrolledToThis
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
                        if (root.wrapper.room)
                            root.wrapper.room.eventShown()
                    }
                }
            }
        }
    }

    HoverHandler {
        id: messageHover
        blocking: false
        onHoveredChanged: root.wrapper.handleMessageHoverChanged(hovered, messageBubble)
    }

    Timer {
        id: hoverDismissTimer
        interval: 180
        repeat: false
        onTriggered: root.wrapper.handleHoverDismissTimerTriggered(messageHover.hovered)
    }

    AbstractButton {
        id: messageBubble

        anchors.horizontalCenter: undefined
        anchors.left: undefined
        anchors.right: undefined
        x: (root.wrapper.isStateEvent || !root.wrapper.messageIsRightAligned) ? 0 : (parent.width - width)
        y: root.wrapper.isStateEvent
            ? Math.round((parent.height - height) / 2)
            : (root.wrapper.alignBubbleToTop ? 0 : (parent.height - height))

        property color roomColor: root.wrapper.resolveUserColor(root.wrapper.userId, root.wrapper.themeBaseColor)
        property var roomBubblePalette: root.wrapper.resolveUserBubblePalette(root.wrapper.userId, roomColor)

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
            property real replyContentWidth: root.wrapper.reply?.implicitWidth ?? 0
            property real mainContentWidth: root.wrapper.main?.implicitWidth ?? 0

            // Cap implicit width to maxWidth so the bubble never overflows
            // its container when litehtml content_width exceeds the render
            // constraint (e.g. <pre> blocks with long lines).
            implicitWidth: Math.min(
                Math.max(replyContentWidth + root.wrapper.replyInset, mainContentWidth + root.wrapper.mainInset),
                root.wrapper.maxWidth
            )
            // Ensure at least one text-line height so that the metadata
            // (bottom-anchored to the bubble) never overflows above the
            // body bounds when the message content is empty.
            implicitHeight: Math.max(contentColumn.implicitHeight, fontMetrics.height)

            Column {
                id: contentColumn
                spacing: Komai.paddingMedium

                anchors.left: parent.left
                anchors.right: parent.right

                AbstractButton {
                    id: replyRow
                    visible: root.wrapper.replyTo

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
                        if (root.wrapper.room && root.wrapper.replyTo) {
                            const modelUserId = root.wrapper.room.dataById(root.wrapper.replyTo, Room.UserId, root.wrapper.eventId);
                            if (typeof modelUserId === "string" && modelUserId.length > 0)
                                return modelUserId;
                        }

                        const delegateUserId = root.wrapper.reply?.userId;
                        return (typeof delegateUserId === "string") ? delegateUserId : "";
                    }
                    property color userColor: root.wrapper.resolveUserColor(replyUserId, root.wrapper.themeWindowColor)
                    property color roomColor: root.wrapper.resolveUserColor(replyUserId, root.wrapper.themeBaseColor)
                    property var replyBubblePalette: root.wrapper.resolveUserBubblePalette(replyUserId, roomColor)

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
                        id: replyCol
                        spacing: 0

                        AbstractButton {
                            id: replyUserButton

                            contentItem: Label {
                                id: userName_
                                text: root.wrapper.reply?.userName ?? "missing name"
                                color: Komai.readableAccentTextColor(replyRow.userColor, replyRow.roomColor)
                                textFormat: Text.RichText
                                width: root.wrapper.maxWidth
                            }
                            onClicked: {
                                if (root.wrapper.room)
                                    root.wrapper.room.openUserProfile(root.wrapper.reply?.userId)
                            }
                        }
                        data: [
                            replyUserButton,
                            root.wrapper.reply,
                        ]
                    }

                    background: Rectangle {
                        color: replyRow.roomColor
                        radius: Komai.paddingMedium
                        clip: true

                        Rectangle {
                            id: replyLine

                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            anchors.left: parent.left
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
                        let link = root.wrapper.reply.hoveredLink;
                        if (link) {
                            Komai.openLink(link)
                        } else {
                            console.log("Scrolling to " + root.wrapper.replyTo);
                            if (root.wrapper.room && typeof root.wrapper.room.showEvent === "function")
                                root.wrapper.room.showEvent(root.wrapper.replyTo)
                            else if (root.wrapper.roomForColorCoding
                                     && typeof root.wrapper.roomForColorCoding.showEvent === "function")
                                root.wrapper.roomForColorCoding.showEvent(root.wrapper.replyTo)
                        }
                    }
                    onPressAndHold: root.wrapper.openReplyContextMenu(root.wrapper.reply, root.wrapper.replyTo, pressX, pressY, replyLine.width, replyUserButton.implicitHeight)
                    TapHandler {
                        acceptedButtons: Qt.RightButton
                        acceptedDevices: PointerDevice.Mouse | PointerDevice.Stylus | PointerDevice.TouchPad
                        gesturePolicy: TapHandler.ReleaseWithinBounds
                        onSingleTapped: eventPoint => root.wrapper.openReplyContextMenu(root.wrapper.reply, root.wrapper.replyTo, eventPoint.position.x, eventPoint.position.y, replyLine.width, replyUserButton.implicitHeight)
                    }

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

                data: [replyRow, root.wrapper.main]
            }
        }

        Binding {
            target: root.wrapper.main
            property: "palette.window"
            when: !!root.wrapper.main
            value: messageBubble.palette.window
        }

        Binding {
            target: root.wrapper.main
            property: "palette.windowText"
            when: !!root.wrapper.main
            value: messageBubble.palette.windowText
        }

        Binding {
            target: root.wrapper.main
            property: "palette.base"
            when: !!root.wrapper.main
            value: messageBubble.palette.base
        }

        Binding {
            target: root.wrapper.main
            property: "palette.alternateBase"
            when: !!root.wrapper.main
            value: messageBubble.palette.alternateBase
        }

        Binding {
            target: root.wrapper.main
            property: "palette.text"
            when: !!root.wrapper.main
            value: messageBubble.palette.text
        }

        Binding {
            target: root.wrapper.main
            property: "palette.brightText"
            when: !!root.wrapper.main
            value: messageBubble.palette.brightText
        }

        Binding {
            target: root.wrapper.main
            property: "palette.buttonText"
            when: !!root.wrapper.main
            value: messageBubble.palette.buttonText
        }

        Binding {
            target: root.wrapper.main
            property: "palette.dark"
            when: !!root.wrapper.main
            value: messageBubble.palette.dark
        }

        Binding {
            target: root.wrapper.main
            property: "palette.highlight"
            when: !!root.wrapper.main
            value: messageBubble.palette.highlight
        }

        Binding {
            target: root.wrapper.main
            property: "palette.highlightedText"
            when: !!root.wrapper.main
            value: messageBubble.palette.highlightedText
        }

        Binding {
            target: root.wrapper.main
            property: "palette.link"
            when: !!root.wrapper.main
            value: messageBubble.palette.link
        }

        Binding {
            target: root.wrapper.main
            property: "palette.inactive.text"
            when: !!root.wrapper.main
            value: messageBubble.palette.buttonText
        }

        Binding {
            target: root.wrapper.main
            property: "palette.inactive.windowText"
            when: !!root.wrapper.main
            value: messageBubble.palette.buttonText
        }

        Binding {
            target: root.wrapper.main
            property: "palette.inactive.buttonText"
            when: !!root.wrapper.main
            value: messageBubble.palette.buttonText
        }

        Binding {
            target: root.wrapper.main
            property: "horizontalAlignment"
            when: root.wrapper.alignMessageTextToSide
                  && !root.wrapper.isStateEvent
                  && !!root.wrapper.main
                  && typeof root.wrapper.main.horizontalAlignment !== "undefined"
            value: root.wrapper.messageIsRightAligned ? Text.AlignRight : Text.AlignLeft
        }

        Binding {
            target: root.wrapper.main
            property: "timelineViewportHeight"
            when: !!root.wrapper.main && typeof root.wrapper.main.timelineViewportHeight !== "undefined"
            value: chat.height
        }

        leftPadding: root.wrapper.isStateEvent ? 0 : root.wrapper.messageBubbleHorizontalPadding
        rightPadding: root.wrapper.isStateEvent ? 0 : root.wrapper.messageBubbleHorizontalPadding
        topPadding: root.wrapper.isStateEvent ? 0 : root.wrapper.messageBubbleVerticalPadding
        bottomPadding: root.wrapper.isStateEvent ? 0 : root.wrapper.messageBubbleVerticalPadding
        background: Rectangle {
            color: (!root.wrapper.isStateEvent && root.wrapper.messageBubbleBackgroundEnabled)
                ? messageBubble.roomColor
                : "transparent"
            radius: root.wrapper.messageBubbleRadius
            border.color: Komai.theme.attention
            border.width: root.wrapper.notificationlevel == MtxEvent.Highlight ? 1 : 0

            TimelineRoundedOutline {
                anchors.fill: parent
                visible: !root.wrapper.isStateEvent && root.wrapper.messageBubbleBackgroundEnabled && root.wrapper.status === MtxEvent.Sent
                borderColor: messageBubble.roomColor
                strokeWidth: 1.5
                dashPattern: [6, 10]
                cornerRadius: root.wrapper.messageBubbleRadius
                inset: 0.75
                animateDashOffset: true
            }
        }
    }

    TimelineMetadata {
        id: metadataOuter

        scaling: 0.9

        visible: !root.wrapper.isStateEvent
            || Settings.timelineMessageActionsActivationPolicy === Settings.TimelineMessageActionsActivationPolicy.ActionsButton

        anchors.bottom: root.wrapper.isStateEvent ? undefined : messageBubble.bottom
        anchors.bottomMargin: root.wrapper.isStateEvent
            ? 0
            : Math.round(Math.max(1, messageBubble.bottomPadding - (metadataOuter.height - fontMetrics.height) / 2))
        anchors.verticalCenter: root.wrapper.isStateEvent ? root.verticalCenter : undefined

        anchors.left: undefined
        anchors.right: undefined
        x: {
            if (root.wrapper.isStateEvent)
                return Math.round(messageBubble.x + messageBubble.width + Komai.paddingSmall);
            if (root.wrapper.pushMetadataToEdge) {
                var threadInset = root.wrapper.threadId ? Komai.paddingSmall : 0;
                return Math.round(root.wrapper.messageIsRightAligned
                    ? threadInset
                    : (root.width - width - threadInset));
            }
            const sideX = root.wrapper.messageIsRightAligned
                ? (messageBubble.x - width - Komai.paddingSmall)
                : (messageBubble.x + messageBubble.width + Komai.paddingSmall);
            return Math.round(sideX);
        }

        eventId: root.wrapper.eventId
        status: root.wrapper.status
        trustlevel: root.wrapper.trustlevel
        isEdited: root.wrapper.isEdited
        isEncrypted: root.wrapper.isEncrypted
        isStateEvent: root.wrapper.isStateEvent
        threadId: root.wrapper.threadId
        timestamp: root.wrapper.timestamp
        room: root.wrapper.room
        isSender: root.wrapper.isStateEvent ? false : root.wrapper.messageIsRightAligned
        actionBarActive: root.wrapper.messageActions.pinned && root.wrapper.messageActions.attached === root.wrapper
        onActionToggled: {
            root.wrapper.togglePinnedMessageActions(metadataOuter.actionToggleButton);
        }
    }

    DragHandler {
        id: replyDragHandler
        enabled: Settings.uiInputTouchSwipeGesturesEnabled
        yAxis.enabled: false
        xAxis.enabled: true
        xAxis.minimum: (root.wrapper.messageIsRightAligned ? 0 : root.wrapper.avatarMargin) - 100
        xAxis.maximum: root.wrapper.messageIsRightAligned ? 0 : root.wrapper.avatarMargin
        onActiveChanged: {
            if (!replyDragHandler.active) {
                if (replyDragHandler.xAxis.minimum <= replyDragHandler.xAxis.activeValue + 1) {
                    if (root.wrapper.room)
                        root.wrapper.room.reply = root.wrapper.eventId;
                }
                root.x = Qt.binding(function () {
                    return root.wrapper.avatarIsOnRight ? 0 : root.wrapper.avatarMargin;
                });
            }
        }
    }

    TapHandler {
        onDoubleTapped: {
            if (root.wrapper.room)
                root.wrapper.room.reply = root.wrapper.eventId
        }
    }

    Item {
        id: selectionToggleSurface

        x: -root.x
        y: 0
        width: root.wrapper.width
        height: root.height
        z: 30
        visible: width > 0 && height > 0

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton
            propagateComposedEvents: false
            preventStealing: true
            cursorShape: undefined

            function isSelectionToggleClick(modifiers) {
                return (Number(modifiers) & (Qt.ControlModifier | Qt.MetaModifier)) !== 0;
            }

            onPressed: mouse => {
                if (!isSelectionToggleClick(mouse.modifiers))
                    mouse.accepted = false;
            }
            onClicked: mouse => {
                if (!isSelectionToggleClick(mouse.modifiers)) {
                    mouse.accepted = false;
                    return;
                }

                root.wrapper.handleMouseSelectionToggle();
            }
        }
    }

    Item {
        anchors.fill: parent
        anchors.topMargin: replyRow.height

        TapHandler {
            acceptedButtons: Qt.RightButton
            acceptedDevices: PointerDevice.Mouse | PointerDevice.Stylus | PointerDevice.TouchPad
            gesturePolicy: TapHandler.ReleaseWithinBounds

            onSingleTapped: event => {
                root.wrapper.openMessageContextMenu(root.wrapper.main.hoveredLink, root.wrapper.main.copyText);
            }
        }
    }

    TimelineRoundedOutline {
        anchors.fill: parent
        z: 2.8
        visible: root.wrapper.selectedInView
        borderColor: root.wrapper.selectedBorderColor
        strokeWidth: 3
        cornerRadius: 8
        inset: 1.5
    }
    TimelineRoundedOutline {
        anchors.fill: parent
        z: 3.2
        visible: root.wrapper.focusedInView
        borderColor: root.wrapper.focusedOutlineColor
        strokeWidth: 3
        dashPattern: [8, 6]
        cornerRadius: 8
        inset: 1.5
    }
    TimelineRoundedOutline {
        anchors.fill: parent
        z: 2
        visible: !!root.wrapper.threadId && !root.wrapper.isStateEvent
        borderColor: threadBackground.threadColor
        strokeWidth: 1.5
        dashPattern: [6, 10]
        cornerRadius: 8
        inset: 0.75
    }
}
