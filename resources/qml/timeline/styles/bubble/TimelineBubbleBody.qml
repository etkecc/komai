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
    // Litehtml-backed delegates (text-like bubbles) own the press for text
    // selection and emit their own modifier-click / drag-select signals. For
    // those rows, the row-level TapHandlers / DragHandler on
    // `selectionToggleSurface` are disabled so they don't claim presses out
    // from under the litehtml's mouse handling.
    readonly property bool mainHasLitehtml: !!(wrapper.main
        && typeof wrapper.main.suppressTextSelection === "function")
    readonly property var metadataItem: metadataLoader.item ? metadataLoader.item : metadataFallback
    property int replyPreviewRevision: 0
    // Dashed thread outline: shown only on a thread root in the main timeline,
    // where it marks a message that has a thread. Inside the thread view every
    // message belongs to the thread, so the outline is pure repetition there;
    // the tinted background and the thread header bar already convey context.
    readonly property bool threadOutlineVisible: wrapper.isThreadRoot
        && !wrapper.isStateEvent
        && !(wrapper.chatRoot && wrapper.chatRoot.threadViewActive)

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
            "stateEventIconColorCategory": String(readReplyRole(Room.StateEventIconColorCategory, "")),
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

            onSingleTapped: eventPoint => {
                root.wrapper.openMessageContextMenu(root.wrapper.main.hoveredLink,
                                                    root.wrapper.main.copyText,
                                                    eventPoint.scenePosition);
            }
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

        // Explicitly triggered rather than a `when`-state transition: a
        // delegate can be created with scrolledToThis already active (a
        // cold event jump instantiates the target's delegate only after
        // the highlight is set, and model resets recreate it mid-flash),
        // and a state that is active at creation applies without ever
        // running its transition — the flash silently never showed.
        property bool flashRequested: root.wrapper.scrolledToThis

        onFlashRequestedChanged: {
            if (flashRequested)
                scrollHighlightAnimation.restart();
        }
        Component.onCompleted: {
            if (flashRequested)
                scrollHighlightAnimation.restart();
        }

        SequentialAnimation {
            id: scrollHighlightAnimation

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
        x: root.wrapper.messageIsRightAligned ? (parent.width - width) : 0
        // For state events, the bubbleBody can be taller than the bubble
        // (the side-anchored metadata's height feeds into bubbleBody's
        // implicitHeight). Center the bubble in that extra space so the
        // state-event text+icon sit at the visual middle of the hover
        // highlight rectangle (#56).
        y: root.wrapper.alignBubbleToTop
            ? (root.wrapper.isStateEvent ? Math.max(0, Math.round((parent.height - height) / 2)) : 0)
            : (parent.height - height)

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
                // State-event text renders at 0.95x and the bubble height is
                // floored to a full text line (and may be raised further by
                // the side-anchored metadata's height). Without centering,
                // the smaller text sits at the top of the hover-highlight
                // rectangle with empty space below (#56).
                anchors.verticalCenter: root.wrapper.isStateEvent ? parent.verticalCenter : undefined

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

        // Drag-select gesture wiring (#124): listen for selection-drag signals
        // on whichever delegate is the bubble's main content (LitehtmlItem
        // for text-like messages; other types simply don't emit). The handler
        // chain ends up in MatrixRoomWalkModeSupport's drag-select controller.
        Connections {
            target: root.wrapper.main
            ignoreUnknownSignals: true

            function onSelectionDragBegan(modifiers) {
                root.wrapper.handleDragSelectBegan(modifiers);
            }
            function onSelectionDragMoved(scenePos) {
                root.wrapper.handleDragSelectMoved(scenePos);
            }
            function onSelectionDragEnded() {
                root.wrapper.handleDragSelectEnded();
            }
            // Modifier-click signals from `LitehtmlItem` — same handlers the
            // row-level Ctrl/Meta/Shift TapHandlers use on non-litehtml rows.
            function onClickedWithCtrlOrMeta() {
                root.wrapper.handleMouseSelectionToggle();
            }
            function onClickedWithShift() {
                root.wrapper.handleMouseSelectionRangeTo();
            }
        }

        // Drag-select initiator for non-litehtml bubbles (#124). Attaching
        // this to the bubble itself (rather than the row-wide
        // `selectionToggleSurface` overlay at z:30) keeps the press visible
        // to delegate-internal MouseAreas like the one in `MediaImageSurface`
        // that opens the media viewer on click. With this nesting:
        //   - Press on the image: MouseArea (deepest) takes the exclusive
        //     grab; the DragHandler here observes via a passive grab.
        //   - Release without movement: MouseArea's onClicked fires →
        //     viewer opens.
        //   - Movement past `dragThreshold`: DragHandler steals the grab
        //     and drives `MatrixRoomWalkModeSupport`'s drag-select.
        //
        // Text bubbles (litehtml) drive the same gesture from inside
        // `LitehtmlItem`, so this handler gates off via `mainHasLitehtml`.
        //
        // `acceptedModifiers` is left at its default (any modifier state):
        // Qt matches it with strict equality, so listing e.g.
        // `Ctrl | Meta | Shift` would only fire when all three are held at
        // once. Reading modifiers via `centroid.modifiers` and letting the
        // walk-mode controller AND-mask for additive handles every
        // combination uniformly.
        DragHandler {
            id: bubbleDragSelect

            target: null
            acceptedButtons: Qt.LeftButton
            acceptedDevices: PointerDevice.Mouse | PointerDevice.Stylus | PointerDevice.TouchPad
            dragThreshold: 12
            enabled: !root.perfDisableTimelineInteraction && !root.mainHasLitehtml && Settings.timelineMessagesDragSelect

            onActiveChanged: {
                if (active) {
                    root.wrapper.handleDragSelectBegan(centroid.modifiers);
                    root.wrapper.handleDragSelectMoved(centroid.scenePosition);
                } else {
                    root.wrapper.handleDragSelectEnded();
                }
            }
            onCentroidChanged: {
                if (active)
                    root.wrapper.handleDragSelectMoved(centroid.scenePosition);
            }
        }

        Binding {
            target: root.wrapper.main
            property: "roomAdapter"
            when: !!root.wrapper.main && typeof root.wrapper.main.roomAdapter !== "undefined"
            value: root.wrapper.effectiveRoomContext
        }

        // Plumb the bubble's content max width into delegates that want to widen beyond their
        // intrinsic size when needed (e.g. ImageMessage uses this to give a long caption a readable
        // pill width even when the image itself is very narrow). Using Binding here matches the
        // pattern for roomAdapter/parent above and ensures the value survives reparenting.
        Binding {
            target: root.wrapper.main
            property: "bubbleMaxWidth"
            when: !!root.wrapper.main && typeof root.wrapper.main.bubbleMaxWidth === "number"
            value: root.wrapper.maxWidth
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
            // Use the `in` operator instead of `typeof main.prop !== "undefined"`.
            // The latter reads the property value and registers a dependency,
            // so when the Binding writes back the dependency re-fires the
            // `when` evaluation and Qt 6 flags it as a binding loop. `in`
            // checks key existence without reading the value.
            when: !!root.wrapper.main && "timelineViewportHeight" in root.wrapper.main
            value: chat.height
        }

        Binding {
            target: root.wrapper.main
            property: "stateEventIconOnRight"
            when: root.wrapper.isStateEvent
                  && !!root.wrapper.main
                  && "stateEventIconOnRight" in root.wrapper.main
            value: root.wrapper.messageIsRightAligned
        }

        leftPadding: root.wrapper.messageBubbleHorizontalPadding
        rightPadding: root.wrapper.messageBubbleHorizontalPadding
        topPadding: root.wrapper.messageBubbleVerticalPadding
        bottomPadding: root.wrapper.messageBubbleVerticalPadding
        background: Rectangle {
            // State events get a subdued tint of the actor's room color when bubble
            // chrome is enabled, so they read as "system-y" while still aligning with
            // regular message bubbles. Plain style disables bubble chrome entirely.
            color: {
                if (!root.wrapper.messageBubbleBackgroundEnabled)
                    return "transparent";
                if (root.wrapper.isStateEvent)
                    return Qt.rgba(messageBubble.roomColor.r,
                                   messageBubble.roomColor.g,
                                   messageBubble.roomColor.b,
                                   0.35);
                return messageBubble.roomColor;
            }
            radius: root.wrapper.messageBubbleRadius
            border.color: Komai.theme.attention
            border.width: root.wrapper.notificationlevel == MtxEvent.Highlight ? 1 : 0

            TimelineRoundedOutline {
                anchors.fill: parent
                visible: !root.wrapper.isStateEvent && root.wrapper.messageBubbleBackgroundEnabled && root.wrapper.status === MtxEvent.Pending
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
            // Center the metadata row vertically with the last line of
            // message text instead of bottom-aligning it.  The row is
            // taller than a text line (driven by buttonSize), so without
            // this correction it sits visually too high.
            readonly property real bubbleBottomMargin: Math.round(Math.max(0, messageBubble.bottomPadding - (visualAnchorHeight - fontMetrics.height) / 2))

            visible: !root.wrapper.isStateEvent
                || Settings.timelineMessageActionsActivationPolicy === Settings.TimelineMessageActionsActivationPolicy.ActionsButton

            x: {
                if (root.wrapper.pushMetadataToEdge) {
                    // Match the dashed thread outline below: only an outlined
                    // message needs the inset that keeps the row-edge bar off
                    // the outline.
                    var threadInset = root.threadOutlineVisible ? Komai.paddingSmall : 0;
                    return Math.round(root.wrapper.messageIsRightAligned
                        ? threadInset
                        : (root.width - width - threadInset));
                }
                const sideX = root.wrapper.messageIsRightAligned
                    ? (messageBubble.x - width - Komai.paddingSmall)
                    : (messageBubble.x + messageBubble.width + Komai.paddingSmall);
                return Math.round(sideX);
            }
            y: Math.max(0, Math.round(messageBubble.y + messageBubble.height - height - bubbleBottomMargin))

            eventId: root.wrapper.eventId
            isLocalEcho: root.wrapper.isLocalEcho
            transactionId: root.wrapper.transactionId
            sendError: root.wrapper.sendError
            isRecoverable: root.wrapper.isRecoverable
            status: root.wrapper.status
            trustlevel: root.wrapper.trustlevel
            messageShield: root.wrapper.messageShield
            typeString: root.wrapper.typeString
            isEdited: root.wrapper.isEdited
            isEncrypted: root.wrapper.isEncrypted
            isStateEvent: root.wrapper.isStateEvent
            threadId: root.wrapper.threadId
            isThreadRoot: root.wrapper.isThreadRoot
            threadReplyCount: root.wrapper.threadReplyCount
            timestamp: root.wrapper.timestamp
            room: root.wrapper.effectiveRoomContext
            isSender: root.wrapper.isStateEvent ? false : root.wrapper.messageIsRightAligned
            actionsEnabled: root.wrapper.metadataActionsEnabled
            actionBarActive: root.wrapper.messageActions.pinned && root.wrapper.messageActions.attached === root.wrapper
            onActionToggled: {
                root.wrapper.togglePinnedMessageActions(actionToggleButton);
            }
            onReadReceiptsRequested: (eventId) => { if (chatRoot) chatRoot.openReadReceiptsDialog(eventId); }
        }
    }

    Loader {
        id: metadataLoader
        // Above `selectionToggleSurface` (z:30) so the metadata bar's
        // click targets (thread button, actions toggle, unpin) win over
        // the gutter's DragHandler that otherwise grabs presses passively
        // on the same pixels and swallows the click. Bare-Item areas of
        // the metadata bar still let presses fall through to the gutter
        // handlers, so drag-select from non-button parts is unaffected.
        z: 31
        active: !root.perfDisableTimelineMetadata
        sourceComponent: metadataComponent
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

        // Row-level modifier-click handlers — fire on Ctrl/Meta/Shift-click for
        // bubbles whose content doesn't drive its own modifier-click signal.
        // Litehtml-backed bubbles (text/notice/emote) are gated off here and
        // emit `clickedWithCtrlOrMeta` / `clickedWithShift` themselves from
        // inside `LitehtmlItem`, so the press flows through to the litehtml
        // and text-selection drag isn't pre-empted by the TapHandlers above.
        // For litehtml rows, the empty space *beside* the bubble is covered
        // by the two gutter Items below.
        TapHandler {
            acceptedButtons: Qt.LeftButton
            acceptedModifiers: Qt.ControlModifier
            acceptedDevices: PointerDevice.Mouse | PointerDevice.Stylus | PointerDevice.TouchPad
            enabled: !root.perfDisableTimelineInteraction && !root.mainHasLitehtml
            gesturePolicy: TapHandler.ReleaseWithinBounds

            onSingleTapped: {
                root.wrapper.handleMouseSelectionToggle();
            }
        }

        TapHandler {
            acceptedButtons: Qt.LeftButton
            acceptedModifiers: Qt.MetaModifier
            acceptedDevices: PointerDevice.Mouse | PointerDevice.Stylus | PointerDevice.TouchPad
            enabled: !root.perfDisableTimelineInteraction && !root.mainHasLitehtml
            gesturePolicy: TapHandler.ReleaseWithinBounds

            onSingleTapped: {
                root.wrapper.handleMouseSelectionToggle();
            }
        }

        TapHandler {
            acceptedButtons: Qt.LeftButton
            acceptedModifiers: Qt.ShiftModifier
            acceptedDevices: PointerDevice.Mouse | PointerDevice.Stylus | PointerDevice.TouchPad
            enabled: !root.perfDisableTimelineInteraction && !root.mainHasLitehtml
            gesturePolicy: TapHandler.ReleaseWithinBounds

            onSingleTapped: {
                root.wrapper.handleMouseSelectionRangeTo();
            }
        }

        // Gutter Items covering the empty space on either side of the bubble.
        // They serve two roles:
        //
        //   1. Modifier-click (Ctrl/Meta/Shift) for litehtml rows. The row-wide
        //      TapHandlers above can't cover the bubble on these rows without
        //      grabbing the press out from under litehtml's text-selection
        //      drag, so those are gated off and the gutters pick up the slack.
        //      For non-litehtml rows the row-wide TapHandlers already cover
        //      everything, so the per-TapHandler `enabled: root.mainHasLitehtml`
        //      below keeps the gutter from double-firing the toggle.
        //
        //   2. Drag-select initiator (#179) for both row types. The bubble's
        //      own DragHandlers and `LitehtmlItem`'s selection drag both stop
        //      at the bubble's edge, so starting a press in the visible empty
        //      space beside the bubble (or in the avatar column) would
        //      otherwise do nothing. The DragHandlers below relay through the
        //      same `handleDragSelectBegan/Moved/Ended` plumbing the bubble
        //      uses, with the row's `eventId` as the gesture's anchor.
        //
        // `messageBubble.x` is in `root` coords; `selectionToggleSurface.x`
        // is `-root.x`, so a point at `root` coord X sits at
        // `X - (-root.x)` = `X + root.x` inside the surface.
        Item {
            id: bubbleGutterLeft

            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            width: Math.max(0, messageBubble.x + root.x)
            enabled: !root.perfDisableTimelineInteraction

            TapHandler {
                acceptedButtons: Qt.LeftButton
                acceptedModifiers: Qt.ControlModifier
                acceptedDevices: PointerDevice.Mouse | PointerDevice.Stylus | PointerDevice.TouchPad
                gesturePolicy: TapHandler.ReleaseWithinBounds
                enabled: root.mainHasLitehtml
                onSingleTapped: root.wrapper.handleMouseSelectionToggle()
            }
            TapHandler {
                acceptedButtons: Qt.LeftButton
                acceptedModifiers: Qt.MetaModifier
                acceptedDevices: PointerDevice.Mouse | PointerDevice.Stylus | PointerDevice.TouchPad
                gesturePolicy: TapHandler.ReleaseWithinBounds
                enabled: root.mainHasLitehtml
                onSingleTapped: root.wrapper.handleMouseSelectionToggle()
            }
            TapHandler {
                acceptedButtons: Qt.LeftButton
                acceptedModifiers: Qt.ShiftModifier
                acceptedDevices: PointerDevice.Mouse | PointerDevice.Stylus | PointerDevice.TouchPad
                gesturePolicy: TapHandler.ReleaseWithinBounds
                enabled: root.mainHasLitehtml
                onSingleTapped: root.wrapper.handleMouseSelectionRangeTo()
            }
            // Single DragHandler accepting any modifier state. Qt's
            // `acceptedModifiers` is matched with strict equality, so
            // `Ctrl | Meta | Shift` would only fire when all three are held
            // at once; passing modifiers in via `centroid.modifiers` and
            // letting the walk-mode controller AND-mask for additive keeps
            // the additive-vs-replace decision honest for any single mod
            // (or combination) the user happens to hold.
            DragHandler {
                target: null
                acceptedButtons: Qt.LeftButton
                acceptedDevices: PointerDevice.Mouse | PointerDevice.Stylus | PointerDevice.TouchPad
                dragThreshold: 12
                enabled: Settings.timelineMessagesDragSelect

                onActiveChanged: {
                    if (active) {
                        root.wrapper.handleDragSelectBegan(centroid.modifiers);
                        root.wrapper.handleDragSelectMoved(centroid.scenePosition);
                    } else {
                        root.wrapper.handleDragSelectEnded();
                    }
                }
                onCentroidChanged: {
                    if (active)
                        root.wrapper.handleDragSelectMoved(centroid.scenePosition);
                }
            }
        }

        Item {
            id: bubbleGutterRight

            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            width: Math.max(0, parent.width - (messageBubble.x + messageBubble.width + root.x))
            enabled: !root.perfDisableTimelineInteraction

            TapHandler {
                acceptedButtons: Qt.LeftButton
                acceptedModifiers: Qt.ControlModifier
                acceptedDevices: PointerDevice.Mouse | PointerDevice.Stylus | PointerDevice.TouchPad
                gesturePolicy: TapHandler.ReleaseWithinBounds
                enabled: root.mainHasLitehtml
                onSingleTapped: root.wrapper.handleMouseSelectionToggle()
            }
            TapHandler {
                acceptedButtons: Qt.LeftButton
                acceptedModifiers: Qt.MetaModifier
                acceptedDevices: PointerDevice.Mouse | PointerDevice.Stylus | PointerDevice.TouchPad
                gesturePolicy: TapHandler.ReleaseWithinBounds
                enabled: root.mainHasLitehtml
                onSingleTapped: root.wrapper.handleMouseSelectionToggle()
            }
            TapHandler {
                acceptedButtons: Qt.LeftButton
                acceptedModifiers: Qt.ShiftModifier
                acceptedDevices: PointerDevice.Mouse | PointerDevice.Stylus | PointerDevice.TouchPad
                gesturePolicy: TapHandler.ReleaseWithinBounds
                enabled: root.mainHasLitehtml
                onSingleTapped: root.wrapper.handleMouseSelectionRangeTo()
            }
            // Single DragHandler accepting any modifier state. Qt's
            // `acceptedModifiers` is matched with strict equality, so
            // `Ctrl | Meta | Shift` would only fire when all three are held
            // at once; passing modifiers in via `centroid.modifiers` and
            // letting the walk-mode controller AND-mask for additive keeps
            // the additive-vs-replace decision honest for any single mod
            // (or combination) the user happens to hold.
            DragHandler {
                target: null
                acceptedButtons: Qt.LeftButton
                acceptedDevices: PointerDevice.Mouse | PointerDevice.Stylus | PointerDevice.TouchPad
                dragThreshold: 12
                enabled: Settings.timelineMessagesDragSelect

                onActiveChanged: {
                    if (active) {
                        root.wrapper.handleDragSelectBegan(centroid.modifiers);
                        root.wrapper.handleDragSelectMoved(centroid.scenePosition);
                    } else {
                        root.wrapper.handleDragSelectEnded();
                    }
                }
                onCentroidChanged: {
                    if (active)
                        root.wrapper.handleDragSelectMoved(centroid.scenePosition);
                }
            }
        }

    }

    Item {
        anchors.fill: parent
        // Skip the reply preview area when one is actually rendered, so
        // `Reply`'s own right-click handler keeps owning that region (it
        // opens `ReplyContextMenu`, not this bubble's menu). When there is
        // no reply, `replyRow` is still laid out with its full implicit
        // height — anchoring topMargin to that unconditionally would shave
        // a phantom band off the top of the bubble where right-clicks have
        // no handler.
        anchors.topMargin: replyRow.visible ? replyRow.height : 0
        // Above `selectionToggleSurface` (z:30) and `metadataLoader` (z:31)
        // so right-clicks anywhere on the bubble win the hit chain ahead of
        // the bubble's internal pointer-handler chain (the AbstractButton +
        // `bubbleDragSelect` + content delegate handlers). Without this,
        // right-clicks on the bubble proper land in `threadBackground`'s
        // z:0 TapHandler — but only intermittently, because they have to
        // race through every handler on `messageBubble`'s subtree first.
        // Mirrors the lift pattern from 58a8580e2.
        z: 32

        TapHandler {
            enabled: !root.perfDisableTimelineInteraction
            acceptedButtons: Qt.RightButton
            acceptedDevices: PointerDevice.Mouse | PointerDevice.Stylus | PointerDevice.TouchPad
            gesturePolicy: TapHandler.ReleaseWithinBounds

            onSingleTapped: eventPoint => {
                root.wrapper.openMessageContextMenu(root.wrapper.main.hoveredLink,
                                                    root.wrapper.main.copyText,
                                                    eventPoint.scenePosition);
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
        visible: root.threadOutlineVisible
        borderColor: threadBackground.threadColor
        strokeWidth: 1.5
        dashPattern: [6, 10]
        cornerRadius: 8
        inset: 0.75
    }
}
