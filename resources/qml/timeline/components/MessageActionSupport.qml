// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import cc.etke.komai

QtObject {
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

    function canSendText(messageModel, roomModel) {
        return !!messageModel
            && !messageModel.isStateEvent
            && !!roomModel
            && roomModel.permissions
            && roomModel.permissions.canSend(MtxEvent.TextMessage);
    }

    function canReact(messageModel, roomModel) {
        return !!messageModel
            && !messageModel.isStateEvent
            && !!roomModel
            && roomModel.permissions
            && roomModel.permissions.canSend(MtxEvent.Reaction);
    }

    function canEdit(messageModel, roomModel) {
        return !!messageModel
            && !!messageModel.isEditable
            && canSendText(messageModel, roomModel);
    }

    function canReply(messageModel, roomModel) {
        return canSendText(messageModel, roomModel);
    }

    function canThread(messageModel, roomModel) {
        return canSendText(messageModel, roomModel);
    }

    function canForward(messageModel) {
        return !!messageModel && isForwardableType(messageModel.type);
    }

    function canGoToMessage(messageModel, filteredTimeline) {
        return !!messageModel
            && !!messageModel.eventId
            && !!filteredTimeline
            && !!filteredTimeline.filterByContent;
    }

    function canRemove(messageModel, roomModel) {
        return !!messageModel
            && !!roomModel
            && roomModel.permissions
            && (roomModel.permissions.canRedact() || messageModel.isSender);
    }

    function canViewRaw(messageModel) {
        return !!messageModel && !!messageModel.eventId;
    }

    function openOptionsDialog(chatRoot, messageModel) {
        if (!chatRoot || !messageModel || !messageModel.eventId)
            return false;

        chatRoot.openMessageActionsDialog(
            messageModel.eventId,
            messageModel.threadId,
            messageModel.type,
            messageModel.isSender,
            messageModel.isEncrypted,
            messageModel.isEditable,
            "",
            messageModel.body || "");
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

    function applyForward(chatRoot, messageModel) {
        if (!chatRoot || !canForward(messageModel))
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

        chatRoot.openRemoveMessageDialog(messageModel.eventId);
        return true;
    }
}
