// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Window
import cc.etke.komai
import "../../../delegates"

Item {
    id: root

    required property var wrapper
    property real topOffset: 0
    property alias hovered: messageHover.hovered
    property alias hoverDismissTimer: hoverDismissTimer
    property alias bubbleItem: messageBubble
    property alias replyItem: replyRow
    readonly property bool perfDisableTimelineHover: TimelineManager.perfUiFlagEnabled("disable_timeline_hover")
    readonly property bool perfDisableTimelineInteraction: TimelineManager.perfUiFlagEnabled("disable_timeline_interaction")
    readonly property bool perfDisableTimelineMetadata: TimelineManager.perfUiFlagEnabled("disable_timeline_metadata")
    readonly property var metadataItem: metadataLoader.item ? metadataLoader.item : metadataFallback
    property int replyPreviewRevision: 0

    width: wrapper.width - wrapper.avatarMargin
    height: implicitHeight
    implicitHeight: Math.max(messageBubble.implicitHeight, metadataItem.visible ? metadataItem.height : 0)
    x: wrapper.avatarIsOnRight ? 0 : wrapper.avatarMargin
    y: topOffset

    function readReplyRole(role, fallbackValue) {
        const roomContext = (root.wrapper.room && typeof root.wrapper.room.dataById === "function")
            ? root.wrapper.room
            : root.wrapper.effectiveRoomContext;
        if (!roomContext || typeof roomContext.dataById !== "function" || !root.wrapper.replyTo)
            return fallbackValue;

        const value = roomContext.dataById(root.wrapper.replyTo, role, root.wrapper.eventId);
        return (value === undefined || value === null) ? fallbackValue : value;
    }

    function mergeReplyPreviewData(target, source) {
        if (!source)
            return;

        for (const key of Object.keys(source)) {
            const value = source[key];
            if (value === undefined || value === null)
                continue;
            if ((target[key] === undefined || target[key] === null)
                    || (typeof target[key] === "string" && String(target[key]).length === 0)) {
                target[key] = value;
            }
        }
    }

    function replyPreviewFallbackData() {
        return {
            "type": readReplyRole(Room.Type, MtxEvent.TextMessage),
            "userId": String(readReplyRole(Room.UserId, "")),
            "userName": String(readReplyRole(Room.UserName, "")),
            "body": String(readReplyRole(Room.Body, "")),
            "formattedBody": String(readReplyRole(Room.FormattedBody, "")),
            "isOnlyEmoji": Number(readReplyRole(Room.IsOnlyEmoji, 0)),
            "url": String(readReplyRole(Room.Url, "")),
            "thumbnailUrl": String(readReplyRole(Room.ThumbnailUrl, "")),
            "duration": Number(readReplyRole(Room.Duration, 0)),
            "blurhash": String(readReplyRole(Room.Blurhash, "")),
            "filename": String(readReplyRole(Room.Filename, "")),
            "filesize": String(readReplyRole(Room.Filesize, "")),
            "filesizeBytes": Number(readReplyRole(Room.FilesizeBytes, 0)),
            "mimetype": String(readReplyRole(Room.MimeType, "")),
            "originalHeight": Number(readReplyRole(Room.OriginalHeight, 0)),
            "originalWidth": Number(readReplyRole(Room.OriginalWidth, 0)),
            "proportionalHeight": Number(readReplyRole(Room.ProportionalHeight, 0)),
            "eventId": String(readReplyRole(Room.EventId, root.wrapper.replyTo)),
            "fileTypeIconSource": String(readReplyRole(Room.FileTypeIconSource, "")),
            "stateEventIconSource": String(readReplyRole(Room.StateEventIconSource, "")),
            "formattedStateEvent": String(readReplyRole(Room.FormattedStateEvent, "")),
            "callType": String(readReplyRole(Room.CallType, "")),
            "isEdited": Boolean(readReplyRole(Room.IsEdited, false)),
            "isEditable": Boolean(readReplyRole(Room.IsEditable, false)),
            "isEncrypted": Boolean(readReplyRole(Room.IsEncrypted, false)),
            "isStateEvent": Boolean(readReplyRole(Room.IsStateEvent, false)),
            "replyTo": String(readReplyRole(Room.ReplyTo, "")),
            "threadId": String(readReplyRole(Room.ThreadId, ""))
        };
    }

    function replyPreviewData() {
        const merged = {};
        const timelineRoom = root.wrapper.room;
        const roomContext = root.wrapper.effectiveRoomContext;

        if (roomContext && typeof roomContext.previewDataForEvent === "function")
            mergeReplyPreviewData(merged, roomContext.previewDataForEvent(root.wrapper.replyTo));

        if (timelineRoom && typeof timelineRoom.previewDataForEvent === "function") {
            // Prefer the shared room-context preview path first. It already
            // powers message actions / composer previews and carries richer
            // normalized media data. The raw timeline fallback is still useful
            // for late-arriving inline summaries, but it must not downgrade a
            // fully classified media preview into sparse text-like data.
            mergeReplyPreviewData(merged,
                                  timelineRoom.previewDataForEvent(root.wrapper.replyTo,
                                                                   root.wrapper.eventId));
        }

        mergeReplyPreviewData(merged, replyPreviewFallbackData());
        return merged;
    }

    function replyPreviewAffectedByRowRange(startRow, endRow) {
        if (!root.wrapper.replyTo || !root.wrapper.room
                || typeof root.wrapper.room.idToIndex !== "function") {
            return false;
        }

        const parentRow = root.wrapper.room.idToIndex(root.wrapper.eventId);
        const replyRow = root.wrapper.room.idToIndex(root.wrapper.replyTo);

        return (parentRow >= startRow && parentRow <= endRow)
            || (replyRow >= startRow && replyRow <= endRow);
    }

    function refreshReplyPreviewIfAffected(topLeft, bottomRight) {
        const startRow = topLeft ? topLeft.row : -1;
        const endRow = bottomRight ? bottomRight.row : -1;
        if (replyPreviewAffectedByRowRange(startRow, endRow))
            replyPreviewRevision += 1;
    }

    Connections {
        target: root.wrapper.room
        ignoreUnknownSignals: true

        function onDataChanged(topLeft, bottomRight, _roles) {
            root.refreshReplyPreviewIfAffected(topLeft, bottomRight);
        }

        function onRowsInserted(_parent, first, last) {
            if (root.replyPreviewAffectedByRowRange(first, last))
                root.replyPreviewRevision += 1;
        }

        function onRowsRemoved() {
            // After removal the old row indices are invalid. Check
            // whether the replied-to event was the one removed.
            if (!root.wrapper.replyTo || !root.wrapper.room
                    || typeof root.wrapper.room.idToIndex !== "function")
                return;
            if (root.wrapper.room.idToIndex(root.wrapper.replyTo) < 0)
                root.replyPreviewRevision += 1;
        }

        function onModelReset() {
            root.replyPreviewRevision += 1;
        }
    }

    Rectangle {
        id: threadBackground
        anchors.fill: parent
        radius: 8
        // For thread replies, color by thread root ID; for thread roots, color by own event ID.
        readonly property string effectiveThreadId: root.wrapper.threadId
            || (root.wrapper.isThreadRoot ? root.wrapper.eventId : "")
        property color threadColor: {
            const _revision = root.wrapper.timelineColorRevision;
            return TimelineManager.userColor(effectiveThreadId, palette.base);
        }
        property color threadBackgroundColor: effectiveThreadId ? Qt.tint(palette.base, Qt.hsla(threadColor.hslHue, 0.7, threadColor.hslLightness, 0.1)) : "transparent"
        color: (!root.perfDisableTimelineHover && Settings.timelineMessagesHoverHighlight && messageHover.hovered)
            ? palette.alternateBase
            : threadBackgroundColor

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
                    duration: 200
                    easing.type: Easing.InOutQuad
                    from: 0
                    properties: "opacity"
                    target: scrollHighlight
                    to: 0.5
                }
                PropertyAnimation {
                    duration: 400
                    easing.type: Easing.InOutQuad
                    from: 0.5
                    properties: "opacity"
                    target: scrollHighlight
                    to: 0
                }
                ScriptAction {
                    script: {
                        if (root.wrapper.effectiveRoomContext)
                            root.wrapper.effectiveRoomContext.eventShown()
                    }
                }
            }
        }
    }

    HoverHandler {
        id: messageHover
        enabled: !root.perfDisableTimelineHover && !root.perfDisableTimelineInteraction
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
            property real replyContentWidth: replyRow.visible ? replyRow.implicitWidth : 0
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

                // Compute reply palette here (not on the Reply item) to avoid
                // a binding loop: Reply sets its own palette.* from bubblePalette,
                // and QML detects a cycle when the computation lives on the same
                // object whose palette is being written.
                property string replyUserId: (replyRow.previewData && replyRow.previewData.userId !== undefined)
                    ? String(replyRow.previewData.userId)
                    : ""
                readonly property bool hasReplyUser: replyUserId.length > 0
                readonly property color resolvedReplyUserColor: hasReplyUser ? root.wrapper.resolveUserColor(replyUserId, root.wrapper.themeWindowColor) : messageBubble.palette.mid
                readonly property color resolvedReplyRoomColor: hasReplyUser ? root.wrapper.resolveUserColor(replyUserId, root.wrapper.themeBaseColor) : messageBubble.palette.mid
                readonly property var resolvedReplyBubblePalette: root.wrapper.resolveUserBubblePalette(
                    hasReplyUser ? replyUserId : root.wrapper.userId,
                    hasReplyUser ? resolvedReplyRoomColor : messageBubble.roomColor)

                Reply {
                    id: replyRow
                    // Hide reply previews for threaded messages. The Matrix thread
                    // protocol always sets m.in_reply_to (often with is_falling_back: true)
                    // for compatibility with clients that don't understand threads.
                    // Even for genuine in-thread replies the preview is redundant in a
                    // flat timeline — the thread icon in the metadata already signals
                    // thread membership. This matches Element X/Web behavior.
                    //
                    // A more precise approach would be to wire the is_falling_back flag
                    // through Rust → C++ model role → QML and only hide fallback replies,
                    // preserving previews for genuine in-thread replies. We skip that for
                    // now to keep the plumbing slim.
                    visible: root.wrapper.replyTo && !root.wrapper.threadId
                    enabled: !root.perfDisableTimelineInteraction
                    anchors.left: parent.left
                    anchors.right: parent.right
                    eventId: root.wrapper.replyTo
                    previewData: {
                        const _ = root.replyPreviewRevision;
                        return root.replyPreviewData();
                    }
                    room_: null
                    roomModelOverride: root.wrapper.effectiveRoomContext
                    timelineViewOverride: (typeof timelineView !== "undefined") ? timelineView : null
                    replyContextMenuOverride: root.wrapper.replyContextMenu
                    keepFullText: true
                    maxWidth: root.wrapper.maxWidth
                    userColor: contentColumn.resolvedReplyUserColor
                    roomColor: contentColumn.resolvedReplyRoomColor
                    bubblePalette: contentColumn.resolvedReplyBubblePalette
                }

            }

            // Reparent the main message delegate into the content column.
            // Using a Binding instead of Column.data avoids disrupting the
            // reply preview's scene-graph node when the main delegate
            // transitions from null to ready.
            Binding {
                target: root.wrapper.main
                property: "parent"
                when: !!root.wrapper.main
                value: contentColumn
            }
        }

        Binding {
            target: root.wrapper.reply
            property: "visible"
            when: !!root.wrapper.reply
            value: false
        }

        Binding {
            target: root.wrapper.main
            property: "roomAdapter"
            when: !!root.wrapper.main && typeof root.wrapper.main.roomAdapter !== "undefined"
            value: root.wrapper.effectiveRoomContext
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
                visible: !root.wrapper.isStateEvent && root.wrapper.messageBubbleBackgroundEnabled && (root.wrapper.status === MtxEvent.Pending || root.wrapper.status === MtxEvent.Sent)
                borderColor: Qt.rgba(messageBubble.palette.windowText.r, messageBubble.palette.windowText.g, messageBubble.palette.windowText.b, 0.4)
                strokeWidth: 1.5
                dashPattern: [6, 10]
                cornerRadius: root.wrapper.messageBubbleRadius
                inset: 0.75
                animateDashOffset: true
            }
        }
    }

    Item {
        id: metadataFallback
        visible: false
        width: 0
        height: 0
    }

    Component {
        id: metadataComponent

        TimelineMetadata {
            scaling: 0.9
            readonly property real visualAnchorHeight: Math.max(buttonSize, indicatorSize)
            readonly property real bubbleBottomMargin: root.wrapper.isStateEvent
                ? 0
                // Center the metadata row vertically with the last line of
                // message text instead of bottom-aligning it.  The row is
                // taller than a text line (driven by buttonSize), so without
                // this correction it sits visually too high.
                : Math.round(Math.max(0, messageBubble.bottomPadding - (visualAnchorHeight - fontMetrics.height) / 2))

            visible: !root.wrapper.isStateEvent
                || Settings.timelineMessageActionsActivationPolicy === Settings.TimelineMessageActionsActivationPolicy.ActionsButton

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
            y: root.wrapper.isStateEvent
                ? Math.round((root.height - height) / 2)
                : Math.max(0, Math.round(messageBubble.y + messageBubble.height - height - bubbleBottomMargin))

            eventId: root.wrapper.eventId
            status: root.wrapper.status
            trustlevel: root.wrapper.trustlevel
            isEdited: root.wrapper.isEdited
            isEncrypted: root.wrapper.isEncrypted
            isStateEvent: root.wrapper.isStateEvent
            threadId: root.wrapper.threadId
            isThreadRoot: root.wrapper.isThreadRoot
            timestamp: root.wrapper.timestamp
            room: root.wrapper.effectiveRoomContext
            isSender: root.wrapper.isStateEvent ? false : root.wrapper.messageIsRightAligned
            actionBarActive: root.wrapper.messageActions.pinned && root.wrapper.messageActions.attached === root.wrapper
            onActionToggled: {
                root.wrapper.togglePinnedMessageActions(actionToggleButton);
            }
            onReadReceiptsRequested: (eventId) => { if (chatRoot) chatRoot.openReadReceiptsDialog(eventId); }
        }
    }

    Loader {
        id: metadataLoader
        active: !root.perfDisableTimelineMetadata
        sourceComponent: metadataComponent
    }

    DragHandler {
        id: replyDragHandler
        enabled: Settings.uiInputTouchSwipeGesturesEnabled && !root.perfDisableTimelineInteraction
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
        enabled: !root.perfDisableTimelineInteraction
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

        TapHandler {
            acceptedButtons: Qt.LeftButton
            acceptedModifiers: Qt.ControlModifier
            acceptedDevices: PointerDevice.Mouse | PointerDevice.Stylus | PointerDevice.TouchPad
            enabled: !root.perfDisableTimelineInteraction
            gesturePolicy: TapHandler.ReleaseWithinBounds

            onSingleTapped: {
                root.wrapper.handleMouseSelectionToggle();
            }
        }

        TapHandler {
            acceptedButtons: Qt.LeftButton
            acceptedModifiers: Qt.MetaModifier
            acceptedDevices: PointerDevice.Mouse | PointerDevice.Stylus | PointerDevice.TouchPad
            enabled: !root.perfDisableTimelineInteraction
            gesturePolicy: TapHandler.ReleaseWithinBounds

            onSingleTapped: {
                root.wrapper.handleMouseSelectionToggle();
            }
        }
    }

    Item {
        anchors.fill: parent
        anchors.topMargin: replyRow.height

        TapHandler {
            enabled: !root.perfDisableTimelineInteraction
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
        visible: (!!root.wrapper.threadId || root.wrapper.isThreadRoot) && !root.wrapper.isStateEvent
        borderColor: threadBackground.threadColor
        strokeWidth: 1.5
        dashPattern: [6, 10]
        cornerRadius: 8
        inset: 0.75
    }
}
