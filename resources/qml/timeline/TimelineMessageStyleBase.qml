// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import im.nheko

TimelineEvent {
    id: wrapper

    ListView.delayRemove: true
    width: chat.delegateMaxWidth
    anchors.horizontalCenter: ListView.view.contentItem.horizontalCenter

    required property var day
    required property bool isSender
    required property int index
    property var previousMessageDay: previousModelData(index + 1, Room.Day, 0)
    property var previousMessageTimestamp: previousModelData(index + 1, Room.Timestamp, new Date(0))
    property bool previousMessageIsStateEvent: previousModelData(index + 1, Room.IsStateEvent, true)
    property string previousMessageUserId: previousModelData(index + 1, Room.UserId, "")

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

    function previousModelData(row, role, fallback) {
        if (row < 0 || row >= chat.count || !chat.model)
            return fallback;

        if (typeof chat.model.dataByIndex === "function")
            return chat.model.dataByIndex(row, role);

        const roleName = roleNameForPreview(role);
        if (!roleName)
            return fallback;

        if (typeof chat.model.get === "function") {
            const entry = chat.model.get(row);
            if (entry && entry[roleName] !== undefined)
                return entry[roleName];
        }

        if (Array.isArray(chat.model)) {
            const entry = chat.model[row];
            if (entry && entry[roleName] !== undefined)
                return entry[roleName];
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

        var actionsParent = messageActions.parent ? messageActions.parent : chat.contentItem;
        var pos = anchorItem.mapToItem(actionsParent, 0, 0);
        var wrapperPos = wrapper.mapToItem(actionsParent, 0, 0);
        var barW = messageActions.implicitWidth;

        // Y: bar opens upward from anchor top
        messageActions.y = pos.y - messageActions.implicitHeight;

        var leftBound = wrapperPos.x + Nheko.paddingLarge;
        var rightBound = wrapperPos.x + wrapper.width - Nheko.paddingLarge;
        var minX = leftBound;
        var maxX = rightBound - barW;
        if (maxX < minX) {
            minX = wrapperPos.x;
            maxX = wrapperPos.x + wrapper.width - barW;
        }
        if (pin) {
            // X (button mode): center on anchor, clamped to delegate bounds
            var centerX = pos.x + anchorItem.width / 2 - barW / 2;
            messageActions.x = Math.max(minX, Math.min(centerX, maxX));
        } else {
            // X (hover mode): align to message side
            messageActions.x = wrapper.messageIsRightAligned ? maxX : minX;
        }
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
        return replyDelegate.linkAt(x - quoteLineWidth - Nheko.paddingSmall, y - replyHeaderHeight);
    }

    function openReplyContextMenu(replyDelegate, replyTo, x, y, quoteLineWidth, replyHeaderHeight) {
        const copyText = replyDelegate ? (replyDelegate.copyText ?? "") : "";
        replyContextMenu.show(copyText, resolveReplyLink(replyDelegate, x, y, quoteLineWidth, replyHeaderHeight), replyTo);
    }

    function avatarImageUrl(userId) {
        if (room)
            return room.avatarUrl(userId).replace("mxc://", "image://MxcImage/");

        const currentUser = Nheko.currentUser;
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
