// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import cc.etke.komai

TimelineEvent {
    id: wrapper

    ListView.delayRemove: true
    width: chat.delegateMaxWidth
    anchors.horizontalCenter: ListView.view.contentItem.horizontalCenter

    required property var day
    required property bool isSender
    required property int index
    property var previousMessageDay: (!room && previewData && previewData.previousDay !== undefined)
        ? previewData.previousDay
        : previousModelData(index + 1, room ? Room.Day : "day", 0)
    property var previousMessageTimestamp: (!room && previewData && previewData.previousTimestamp !== undefined)
        ? previewData.previousTimestamp
        : previousModelData(index + 1, room ? Room.Timestamp : "timestamp", new Date(0))
    property bool previousMessageIsStateEvent: (!room && previewData && previewData.previousIsStateEvent !== undefined)
        ? previewData.previousIsStateEvent
        : previousModelData(index + 1, room ? Room.IsStateEvent : "isStateEvent", true)
    property string previousMessageUserId: (!room && previewData && previewData.previousUserId !== undefined)
        ? previewData.previousUserId
        : previousModelData(index + 1, room ? Room.UserId : "userId", "")

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
    // Optional preview payload used when no TimelineModel room is available.
    property var previewData: ({})
    readonly property var roomForColorCoding: room ? room : ((previewData && previewData.room) ? previewData.room : null)
    readonly property string roomIdForColorCoding: (roomForColorCoding && roomForColorCoding.roomId) ? String(roomForColorCoding.roomId) : ""

    property var hoverDismissTimerRef: null

    property int oneHour: 60 * 60 * 1000
    property bool showSection: wrapper.previousMessageDay !== wrapper.day || wrapper.timestamp - wrapper.previousMessageTimestamp > oneHour
    readonly property bool startsNewMessageGroup: wrapper.previousMessageUserId !== wrapper.userId
        || wrapper.showSection
        || wrapper.previousMessageIsStateEvent !== wrapper.isStateEvent
    readonly property bool hasRoom: wrapper.room !== null
    readonly property bool messageIsRightAligned: {
        switch (Settings.timelineMessagesPositioning) {
        case Settings.TimelineMessagesPositioning.AllLeft:
            return false;
        case Settings.TimelineMessagesPositioning.AllRight:
            return true;
        case Settings.TimelineMessagesPositioning.OpposingBySender:
        default:
            return wrapper.isSender;
        }
    }

    function roleNameForPreview(role) {
        switch (role) {
        case Room.Day:
            return "day";
        case Room.Timestamp:
            return "timestamp";
        case Room.IsStateEvent:
            return "isStateEvent";
        case Room.UserId:
            return "userId";
        default:
            return "";
        }
    }

    function roleValueForName(roleName) {
        switch (roleName) {
        case "day":
            return Room.Day;
        case "timestamp":
            return Room.Timestamp;
        case "isStateEvent":
            return Room.IsStateEvent;
        case "userId":
            return Room.UserId;
        default:
            return -1;
        }
    }

    function resolveUserColor(targetUserId, backgroundColor, accentColor) {
        if (roomIdForColorCoding.length > 0) {
            return TimelineManager.roomUserColor(
                        roomIdForColorCoding,
                        targetUserId,
                        backgroundColor,
                        accentColor,
                        Settings.timelineUserColorCodingPolicy);
        }

        return TimelineManager.userColor(targetUserId, backgroundColor);
    }

    function previousModelData(row, roleOrName, fallback) {
        if (row < 0 || row >= chat.count || !chat.model)
            return fallback;

        const roleName = typeof roleOrName === "string" ? roleOrName : roleNameForPreview(roleOrName);
        if (!roleName)
            return fallback;

        if (typeof chat.model.dataByIndex === "function") {
            const role = typeof roleOrName === "number" ? roleOrName : roleValueForName(roleName);
            if (role >= 0)
                return chat.model.dataByIndex(row, role);
            return fallback;
        }

        if (typeof chat.model.get === "function") {
            const entry = chat.model.get(row);
            const source = (entry && entry.modelData !== undefined) ? entry.modelData
                            : (entry && entry.model !== undefined) ? entry.model
                            : entry;
            if (source && source[roleName] !== undefined)
                return source[roleName];
        }

        if (Array.isArray(chat.model)) {
            const entry = chat.model[row];
            const source = (entry && entry.modelData !== undefined) ? entry.modelData
                            : (entry && entry.model !== undefined) ? entry.model
                            : entry;
            if (source && source[roleName] !== undefined)
                return source[roleName];
        }

        return fallback;
    }

    function openMessageActions(pin, anchorItem) {
        if (!anchorItem)
            return;

        if (hoverDismissTimerRef)
            hoverDismissTimerRef.stop();

        messageActions.model = wrapper;
        messageActions.attached = wrapper;
        messageActions.pinned = pin;
        messageActions.anchorItem = anchorItem;
        messageActions.positioned = false;

        // Positioning needs a deferred pass because message action intrinsic size is only
        // reliable after the control becomes visible and finishes a layout pass.
        // On first show after app startup, immediate mapToItem/implicitWidth reads can use
        // partial geometry and place the bar outside the visible viewport.
        Qt.callLater(function() {
            wrapper.repositionMessageActions(anchorItem, pin, 0);
        });
    }

    function repositionMessageActions(anchorItem, pinnedState, attempt) {
        if (!anchorItem)
            return;

        if (attempt === undefined)
            attempt = 0;

        // Give the popup a few frames to settle before forcing coordinates.
        if (attempt > 60)
            return;

        var nextAttempt = attempt + 1;

        var actionsParent = messageActions.parent ? messageActions.parent : chat.contentItem;
        if (!actionsParent) {
            Qt.callLater(function () { wrapper.repositionMessageActions(anchorItem, pinnedState, nextAttempt); });
            return;
        }

        var pos = anchorItem.mapToItem(actionsParent, 0, 0);
        var wrapperPos = wrapper.mapToItem(actionsParent, 0, 0);
        var barW = messageActions.implicitWidth;
        var barH = messageActions.implicitHeight;
        var chatWidth = chat.width;
        var chatHeight = chat.height;

        // If intrinsic size is not ready yet, retry on the next frame.
        // If target view metrics are still zero (e.g. very first frame), retry as well.
        if (barW <= 0 || barH <= 0 || chatWidth <= 0 || chatHeight <= 0) {
            // Keep retrying until the control resolves its intrinsic size; we need
            // a stable width/height before showing to avoid a first-run clipped draw.
            Qt.callLater(function () { wrapper.repositionMessageActions(anchorItem, pinnedState, nextAttempt); });
            return;
        }

        // Y: bar opens upward from anchor top.
        // Use chat.contentY/height as the source of truth for visible viewport
        // bounds in content-item coordinates to avoid transform/sign surprises.
        var viewportTop = actionsParent === chat.contentItem ? chat.contentY : 0;
        var viewportBottom = viewportTop + chatHeight;
        var targetY = pos.y - barH;
        messageActions.y = Math.max(viewportTop, Math.min(targetY, viewportBottom - barH));

        // X: compute message bounds first, then clamp final X to viewport width as a final safety.
        // (chat coordinates are stable here because the bar stays in chat.contentItem.)
        var viewportLeft = 0;
        var viewportRight = chatWidth;

        var leftBound = wrapperPos.x + Komai.paddingLarge;
        var rightBound = wrapperPos.x + wrapper.width - Komai.paddingLarge;
        var minX = leftBound;
        var maxX = rightBound - barW;
        if (maxX < minX) {
            // If the bar is wider than the message wrapper, clamping to wrapper
            // bounds forces it to the left edge. Fall back to viewport bounds so
            // pinned positioning still tracks the clicked action button.
            minX = viewportLeft;
            maxX = viewportRight - barW;
        }
        if (pinnedState) {
            // X (button mode): center on anchor, clamped to delegate bounds
            var centerX = pos.x + anchorItem.width / 2 - barW / 2;
            messageActions.x = Math.max(minX, Math.min(centerX, maxX));
        } else {
            // X (hover mode): align to message side
            messageActions.x = wrapper.messageIsRightAligned ? maxX : minX;
        }

        // Extra safety clamp in case width calculations drift during the first few frames.
        messageActions.x = Math.max(viewportLeft, Math.min(messageActions.x, viewportRight - barW));
        messageActions.positioned = true;

    }

    function isHoverActionsEnabled() {
        return Settings.timelineMessageActionsActivationPolicy === Settings.TimelineMessageActionsActivationPolicy.OnHover;
    }

    function isUnpinnedActionBarAttached() {
        return messageActions.attached === wrapper && !messageActions.pinned;
    }

    function handleMessageHoverChanged(isHovered, anchorItem) {
        if (!isHoverActionsEnabled())
            return;

        if (isHovered) {
            if (hoverDismissTimerRef)
                hoverDismissTimerRef.stop();
            openMessageActions(false, anchorItem);
        } else if (isUnpinnedActionBarAttached() && hoverDismissTimerRef) {
            hoverDismissTimerRef.restart();
        }
    }

    function handleHoverDismissTimerTriggered(isHovered) {
        if (!isHoverActionsEnabled())
            return;
        if (!isUnpinnedActionBarAttached())
            return;
        if (isHovered || messageActions.hovered)
            return;
        messageActions.dismiss();
    }

    function togglePinnedMessageActions(anchorItem) {
        if (messageActions.pinned && messageActions.attached === wrapper)
            messageActions.dismiss();
        else
            openMessageActions(true, anchorItem);
    }

    function openMessageContextMenu(hoveredLink, copyText) {
        messageContextMenu.show(eventId, threadId, type, isSender, isEncrypted, isEditable, hoveredLink, copyText);
    }

    function resolveReplyLink(replyDelegate, x, y, quoteLineWidth, replyHeaderHeight) {
        if (!replyDelegate || !replyDelegate.linkAt)
            return "";
        return replyDelegate.linkAt(x - quoteLineWidth - Komai.paddingSmall, y - replyHeaderHeight);
    }

    function openReplyContextMenu(replyDelegate, replyTo, x, y, quoteLineWidth, replyHeaderHeight) {
        const copyText = replyDelegate ? (replyDelegate.copyText ?? "") : "";
        replyContextMenu.show(copyText, resolveReplyLink(replyDelegate, x, y, quoteLineWidth, replyHeaderHeight), replyTo);
    }

    function avatarImageUrl(userId) {
        if (room)
            return room.avatarUrl(userId).replace("mxc://", "image://MxcImage/");

        const currentUser = Komai.currentUser;
        if (currentUser && currentUser.userid == userId) {
            const currentAvatarUrl = currentUser.avatarUrl ? String(currentUser.avatarUrl) : "";
            if (currentAvatarUrl.length > 0)
                return currentAvatarUrl.replace("mxc://", "image://MxcImage/");
        }

        const previewAvatarUrl = (previewData && previewData.avatarUrl) ? String(previewData.avatarUrl) : "";
        if (previewAvatarUrl.length > 0)
            return previewAvatarUrl.replace("mxc://", "image://MxcImage/");

        return "";
    }
}
