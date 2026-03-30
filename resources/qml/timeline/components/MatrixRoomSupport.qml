// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import cc.etke.komai

Item {
    id: support

    required property var rootItem
    required property var roomPreview
    required property var chatRoot
    required property var timelineRoot
    required property var emojiPopup
    required property var filteredTimeline
    required property var timelineList

    width: 0
    height: 0

    readonly property var uploadsController: composerSupport.uploadsController
    readonly property var composerInputController: composerSupport.composerInputController
    readonly property var composerRoom: composerSupport.composerRoom
    readonly property var messageActionsDefaultRoomModel: matrixMessageActionsDefaultRoomModel
    readonly property var messageContextMenu: dialogSupport.messageContextMenu
    readonly property var replyContextMenu: dialogSupport.replyContextMenu
    readonly property var messageActionsHost: dialogSupport.messageActionsHost
    readonly property var dialogRoomModel: matrixDialogRoomModel
    readonly property var forwardRoomModel: matrixForwardRoomModel
    readonly property var headerRoomModel: matrixHeaderRoomModel

    MatrixRoomComposerSupport {
        id: composerSupport

        rootItem: support.rootItem
        roomPreview: support.roomPreview
        dialogRoomModel: matrixDialogRoomModel
    }

    PreviewPermissions {
        id: matrixMessageActionsDefaultPermissions
    }

    QtObject {
        id: matrixMessageActionsDefaultRoomModel

        property string roomId: roomPreview ? roomPreview.roomid : ""
        property bool isActiveMatrixTimelineRoom: true
        property int roomMemberCount: roomPreview && roomPreview.roomMemberCount !== undefined
            ? Number(roomPreview.roomMemberCount) : 0
        property bool isEncrypted: roomPreview ? !!roomPreview.isEncrypted : false
        property var permissions: matrixMessageActionsDefaultPermissions
        property var input: matrixTimelineToolbarInput
        property var frequentReactions: []
        property var pinnedMessages: TimelineManager.matrixTimelinePinnedEventIds
        property string reply: ""
        property string edit: ""
        property string thread: ""
        property bool supportsThreadNavigation: false

        function showEvent(eventId) {
            return rootItem.jumpToLoadedMatrixEvent(eventId);
        }

        function openForwardDialog(eventId) {
            return support.openMatrixForwardDialog(eventId);
        }

        function formatRedactedEvent(eventId) {
            const model = TimelineManager.matrixTimelineModel;
            if (!model) return rootItem.matrixRedactedEventPair("", "");
            const eid = String(eventId || "");
            return rootItem.matrixRedactedEventPair(
                model.userNameForEvent(eid), model.userIdForEvent(eid));
        }

        function previewDataForEvent(eventId) {
            const preview = matrixHeaderRoomModel.previewDataForEvent(eventId);
            return Object.assign({}, preview || {}, {
                "room": matrixMessageActionsDefaultRoomModel
            });
        }

        function formatDateSeparator(timestamp) {
            return Qt.formatDate(timestamp, "ddd, MMM d");
        }

        function formatLaterSeparator(_previous, currentTimestamp) {
            return Qt.formatTime(currentTimestamp, "hh:mm");
        }

        function openUserProfile(userId) {
            matrixDialogRoomModel.openUserProfile(userId);
        }

        function eventShown() {}
        function showImage() { return true; }

        function openMedia(targetEventId) {
            const model = TimelineManager.matrixTimelineModel;
            const eid = String(targetEventId || "");
            const fileName = model ? (model.filenameForEvent(eid) || qsTr("Attachment")) : qsTr("Attachment");
            TimelineManager.openActiveMatrixTimelineMedia(eid, fileName);
        }

        function saveMedia(targetEventId) {
            const model = TimelineManager.matrixTimelineModel;
            const eid = String(targetEventId || "");
            const fileName = model ? (model.filenameForEvent(eid) || qsTr("Attachment")) : qsTr("Attachment");
            TimelineManager.saveActiveMatrixTimelineMedia(eid, fileName);
        }

        function copyLinkToEvent(eventId) {
            TimelineManager.copyMatrixEventLink(roomId, String(eventId || ""));
        }

        function markEventAsRead(eventId) {
            TimelineManager.markActiveMatrixTimelineEventAsRead(String(eventId || ""));
        }

        function pin(eventId) {
            TimelineManager.pinActiveMatrixTimelineEvent(String(eventId || ""));
        }

        function unpin(eventId) {
            TimelineManager.unpinActiveMatrixTimelineEvent(String(eventId || ""));
        }

        function reportEvent(eventId, reason, score) {
            TimelineManager.reportActiveMatrixTimelineEvent(
                String(eventId || ""), String(reason || ""), Number(score || -50));
        }

        function viewRawMessage(eventId) {
            rootItem.openRawMessageDialog(String(eventId || ""));
        }

        function viewDecryptedRawMessage(eventId) {
            viewRawMessage(eventId);
        }

        function showReadReceipts(eventId) {
            rootItem.openReadReceiptsDialog(String(eventId || ""));
        }

        onReplyChanged: {
            if (!reply) return;
            const model = TimelineManager.matrixTimelineModel;
            TimelineManager.queueActiveMatrixReply(
                reply,
                model ? model.userIdForEvent(reply) : "",
                model ? model.userNameForEvent(reply) : "",
                model ? model.bodyForEvent(reply) : "");
            reply = "";
        }

        onEditChanged: {
            if (!edit) return;
            const model = TimelineManager.matrixTimelineModel;
            rootItem.beginEdit(edit,
                model ? model.bodyForEvent(edit) : "",
                model ? model.typeStringForEvent(edit) : "");
            edit = "";
        }

        onThreadChanged: {
            if (thread) thread = "";
        }
    }

    QtObject {
        id: matrixTimelineToolbarInput

        function reaction(targetEventId, reactionKey) {
            TimelineManager.toggleActiveMatrixTimelineReaction(
                String(targetEventId || ""), String(reactionKey || ""));
        }
    }

    MatrixRoomDialogSupport {
        id: dialogSupport

        rootItem: support.rootItem
        roomPreview: support.roomPreview
        chatRoot: support.chatRoot
        timelineRoot: support.timelineRoot
        emojiPopup: support.emojiPopup
        filteredTimeline: support.filteredTimeline
        timelineList: support.timelineList
        messageActionsDefaultRoomModel: matrixMessageActionsDefaultRoomModel
        dialogRoomModel: matrixDialogRoomModel
        forwardRoomModel: matrixForwardRoomModel
    }

    QtObject {
        id: matrixDialogRoomModel

        property string roomId: roomPreview ? roomPreview.roomid : ""

        function openUserProfile(userId) {
            const trimmedUserId = String(userId || "").trim();
            if (trimmedUserId.length === 0)
                return;

            TimelineManager.openGlobalUserProfile(trimmedUserId);
        }
    }

    QtObject {
        id: matrixForwardRoomModel

        property string roomId: roomPreview ? roomPreview.roomid : ""

        function forwardMessage(eventId, targetRoomId) {
            TimelineManager.forwardActiveMatrixTimelineEvent(String(eventId || ""),
                                                            String(targetRoomId || ""));
        }
    }

    function openRemoveMessageDialog(eventId) {
        return dialogSupport.openRemoveMessageDialog(eventId);
    }

    function destroyOnClose(dialog) {
        return dialogSupport.destroyOnClose(dialog);
    }

    function openRawMessageDialog(eventId) {
        return dialogSupport.openRawMessageDialog(eventId);
    }

    function openReadReceiptsDialog(eventId) {
        return dialogSupport.openReadReceiptsDialog(eventId);
    }

    function openMatrixForwardDialog(eventId) {
        return dialogSupport.openMatrixForwardDialog(eventId);
    }

    function openReportMessageDialog(eventId) {
        return dialogSupport.openReportMessageDialog(eventId);
    }

    function openMessageActionsDialog(eventId,
                                      threadId,
                                      eventType,
                                      isSender,
                                      isEncrypted,
                                      isEditable,
                                      link,
                                      text,
                                      messageModelOverride,
                                      roomModelOverride) {
        return dialogSupport.openMessageActionsDialog(eventId,
                                                      threadId,
                                                      eventType,
                                                      isSender,
                                                      isEncrypted,
                                                      isEditable,
                                                      link,
                                                      text,
                                                      messageModelOverride,
                                                      roomModelOverride);
    }

    MatrixRoomHeaderModel {
        id: matrixHeaderRoomModel

        rootItem: support.rootItem
        roomPreview: support.roomPreview
        dialogRoomModel: matrixDialogRoomModel
    }
}
