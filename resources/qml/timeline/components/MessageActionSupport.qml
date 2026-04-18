// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import cc.etke.komai

QtObject {
    function permissionsRevision(roomModel) {
        return roomModel && roomModel.permissions && roomModel.permissions.revision !== undefined
            ? roomModel.permissions.revision
            : 0;
    }

    function roomHasMethod(roomModel, methodName) {
        return !!roomModel && typeof roomModel[methodName] === "function";
    }

    function roomCanSend(roomModel, eventType) {
        const _ = permissionsRevision(roomModel);
        return !!roomModel
            && !!roomModel.permissions
            && typeof roomModel.permissions.canSend === "function"
            && roomModel.permissions.canSend(eventType);
    }

    function roomCanChange(roomModel, eventType) {
        const _ = permissionsRevision(roomModel);
        return !!roomModel
            && !!roomModel.permissions
            && typeof roomModel.permissions.canChange === "function"
            && roomModel.permissions.canChange(eventType);
    }

    function actionCapability(messageModel, propertyName, fallbackValue) {
        if (!messageModel)
            return fallbackValue;

        const value = messageModel[propertyName];
        return value === undefined || value === null ? fallbackValue : !!value;
    }

    function isForwardableType(eventType) {
        return eventType == MtxEvent.ImageMessage
            || eventType == MtxEvent.VideoMessage
            || eventType == MtxEvent.AudioMessage
            || eventType == MtxEvent.FileMessage
            || eventType == MtxEvent.Sticker
            || eventType == MtxEvent.TextMessage
            || eventType == MtxEvent.LocationMessage
            || eventType == MtxEvent.EmoteMessage
            || eventType == MtxEvent.NoticeMessage;
    }

    function isMediaType(eventType) {
        return eventType == MtxEvent.ImageMessage
            || eventType == MtxEvent.VideoMessage
            || eventType == MtxEvent.AudioMessage
            || eventType == MtxEvent.FileMessage
            || eventType == MtxEvent.Sticker;
    }

    function canSendText(messageModel, roomModel) {
        return !!messageModel
            && !messageModel.isStateEvent
            && actionCapability(messageModel, "supportsSendText", true)
            && roomCanSend(roomModel, MtxEvent.TextMessage);
    }

    function canReact(messageModel, roomModel) {
        return !!messageModel
            && !messageModel.isLocalEcho
            && !messageModel.isStateEvent
            && actionCapability(messageModel, "supportsReaction", true)
            && roomCanSend(roomModel, MtxEvent.Reaction);
    }

    function canEdit(messageModel, roomModel) {
        return !!messageModel
            && !messageModel.isLocalEcho
            && !!messageModel.isEditable
            && actionCapability(messageModel, "supportsEdit", true)
            && canSendText(messageModel, roomModel);
    }

    function canReply(messageModel, roomModel) {
        return !!messageModel
            && !messageModel.isLocalEcho
            && actionCapability(messageModel, "supportsReply", true)
            && canSendText(messageModel, roomModel);
    }

    function canThread(messageModel, roomModel) {
        return !!messageModel
            && !messageModel.isLocalEcho
            && actionCapability(messageModel, "supportsThread", true)
            && canSendText(messageModel, roomModel);
    }

    function canForward(messageModel) {
        return !!messageModel
            && !messageModel.isLocalEcho
            && actionCapability(messageModel, "supportsForward", true)
            && isForwardableType(messageModel.type);
    }

    function canGoToMessage(messageModel, filteredTimeline) {
        return !!messageModel
            && !messageModel.isLocalEcho
            && !!messageModel.eventId
            && actionCapability(messageModel, "supportsGoToMessage", true)
            && !!filteredTimeline
            && !!filteredTimeline.filterByContent;
    }

    function canOpenOptions(messageModel) {
        return !!messageModel
            && (!!messageModel.eventId || !!messageModel.transactionId)
            && actionCapability(messageModel, "supportsOptions", true);
    }

    function canRemove(messageModel, roomModel) {
        const _ = permissionsRevision(roomModel);
        if (!messageModel
                || !actionCapability(messageModel, "supportsRemove", true))
            return false;
        // Local echoes never reached the server — cancelling is a local queue op,
        // no redact permission required. Only gate on ownership instead.
        if (messageModel.isLocalEcho)
            return true;
        return !!roomModel
            && roomModel.permissions
            && (roomModel.permissions.canRedact() || messageModel.isSender);
    }

    function canViewRaw(messageModel) {
        return !!messageModel
            && !messageModel.isLocalEcho
            && !!messageModel.eventId
            && actionCapability(messageModel, "supportsViewRaw", true);
    }

    function canPin(messageModel, roomModel) {
        return !!messageModel
            && !messageModel.isLocalEcho
            && !!messageModel.eventId
            && actionCapability(messageModel, "supportsPin", true)
            && roomCanChange(roomModel, MtxEvent.PinnedEvents);
    }

    function canReadReceipts(messageModel, roomModel) {
        return !!messageModel
            && !messageModel.isLocalEcho
            && !!messageModel.eventId
            && actionCapability(messageModel, "supportsReadReceipts", true)
            && roomHasMethod(roomModel, "showReadReceipts");
    }

    function canReport(messageModel, chatRoot) {
        return !!messageModel
            && !messageModel.isLocalEcho
            && !!messageModel.eventId
            && actionCapability(messageModel, "supportsReport", true)
            && !!chatRoot
            && typeof chatRoot.showDialogFromComponent === "function";
    }

    function canSaveMedia(messageModel, roomModel) {
        return !!messageModel
            && !messageModel.isLocalEcho
            && actionCapability(messageModel, "supportsSaveMedia", true)
            && isMediaType(messageModel.type)
            && roomHasMethod(roomModel, "saveMedia");
    }

    function canOpenMedia(messageModel, roomModel) {
        return !!messageModel
            && !messageModel.isLocalEcho
            && actionCapability(messageModel, "supportsOpenMedia", true)
            && isMediaType(messageModel.type)
            && roomHasMethod(roomModel, "openMedia");
    }

    function canCopyEventLink(messageModel, roomModel) {
        return !!messageModel
            && !messageModel.isLocalEcho
            && !!messageModel.eventId
            && actionCapability(messageModel, "supportsCopyEventLink", true)
            && roomHasMethod(roomModel, "copyLinkToEvent");
    }

    function openOptionsDialog(chatRoot, messageModel, roomModelOverride) {
        if (!chatRoot || !canOpenOptions(messageModel))
            return false;

        const effectiveRoomModel = roomModelOverride
            ? roomModelOverride
            : ((messageModel && messageModel.roomModelOverride) ? messageModel.roomModelOverride : null);
        // Prefer `realEventId` (empty for local echoes) over `eventId` (which is
        // the content-lookup key — may hold the row's `itemId` fallback, so a
        // failed local echo would otherwise look like a real remote event to the
        // dialog and every !isLocalEcho gate would open up).
        const actualEventId = messageModel.realEventId !== undefined
            ? String(messageModel.realEventId || "")
            : String(messageModel.eventId || "");
        chatRoot.openMessageActionsDialog(
            actualEventId,
            messageModel.threadId,
            messageModel.type,
            messageModel.isSender,
            messageModel.isEncrypted,
            messageModel.isEditable,
            "",
            messageModel.body || "",
            messageModel,
            effectiveRoomModel,
            messageModel.transactionId || "");
        return true;
    }

    function applyEdit(roomModel, messageModel) {
        if (!canEdit(messageModel, roomModel))
            return false;

        roomModel.edit = messageModel.eventId;
        TimelineManager.focusMessageInput();
        return true;
    }

    function applyThread(roomModel, messageModel) {
        if (!canThread(messageModel, roomModel))
            return false;

        roomModel.thread = messageModel.threadId || messageModel.eventId;
        TimelineManager.focusMessageInput();
        return true;
    }

    function applyReply(roomModel, messageModel) {
        if (!canReply(messageModel, roomModel))
            return false;

        roomModel.reply = messageModel.eventId;
        TimelineManager.focusMessageInput();
        return true;
    }

    function applyForward(chatRoot, roomModel, messageModel) {
        if (!canForward(messageModel))
            return false;

        if (roomHasMethod(roomModel, "openForwardDialog")) {
            roomModel.openForwardDialog(messageModel.eventId);
            return true;
        }

        if (!chatRoot || typeof chatRoot.openForwardDialog !== "function")
            return false;

        chatRoot.openForwardDialog(messageModel.eventId);
        return true;
    }

    function applyGoToMessage(chatRoot, roomModel, filteredTimeline, messageModel) {
        if (!chatRoot || !roomModel || !canGoToMessage(messageModel, filteredTimeline))
            return false;

        chatRoot.clearSearch();
        roomModel.showEvent(messageModel.eventId);
        return true;
    }

    function applyViewRaw(roomModel, messageModel) {
        if (!roomModel || !canViewRaw(messageModel))
            return false;

        roomModel.viewRawMessage(messageModel.eventId);
        return true;
    }

    function applyRemove(chatRoot, roomModel, messageModel) {
        if (!chatRoot || !canRemove(messageModel, roomModel))
            return false;

        chatRoot.openRemoveMessageDialog(messageModel.eventId || "",
                                         messageModel.transactionId || "");
        return true;
    }
}
