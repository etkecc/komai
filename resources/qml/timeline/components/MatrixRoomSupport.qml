// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Window
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

    readonly property var uploadsController: matrixUploadsController
    readonly property var composerInputController: matrixComposerInputController
    readonly property var composerRoom: matrixComposerRoom
    readonly property var messageActionsDefaultRoomModel: matrixMessageActionsDefaultRoomModel
    readonly property var messageContextMenu: dialogSupport.messageContextMenu
    readonly property var replyContextMenu: dialogSupport.replyContextMenu
    readonly property var messageActionsHost: dialogSupport.messageActionsHost
    readonly property var dialogRoomModel: matrixDialogRoomModel
    readonly property var forwardRoomModel: matrixForwardRoomModel
    readonly property var headerRoomModel: matrixHeaderRoomModel

    QtObject {
        id: matrixUploadsController

        property var uploads: TimelineManager.matrixTimelineAttachments

        function declineUploads() {
            TimelineManager.clearActiveMatrixAttachments();
        }

        function removeUpload(index) {
            TimelineManager.removeActiveMatrixAttachment(index);
        }

        function send() {
            return TimelineManager.sendActiveMatrixAttachments();
        }
    }

    QtObject {
        id: matrixComposerInputController

        property var uploads: TimelineManager.matrixTimelineAttachments
        readonly property bool uploading: TimelineManager.matrixTimelineAttachmentSending
        property string text: ""
        property string commandValidationMessage: ""
        property string commandValidationState: "none"

        function setText(value) {
            text = String(value || "");
        }

        function openFileSelection() {
            return TimelineManager.openActiveMatrixAttachmentSelection();
        }

        function send() {
            return rootItem.trySendMessage();
        }

        function previousText() {
            return text;
        }

        function nextText() {
            return text;
        }

        function updateState(_selectionStart, _selectionEnd, _cursorPosition, value) {
            const normalized = String(value || "");
            if (text !== normalized)
                text = normalized;
        }

        function clipboardText() {
            return Clipboard.text;
        }

        function tryPasteAttachment(_strict) {
            return false;
        }

        function commandCompletionSearchString(prefix, _cursorPosition) {
            return String(prefix || "");
        }

        function applyCommandCompletion(currentText, _cursorPosition, _completion) {
            return String(currentText || "");
        }

        function commandCompletionCursorPosition(_currentText, cursorPosition, _completion) {
            return cursorPosition;
        }

        function addMention(_userId, _completion) {
        }

        function sticker(_row) {
        }
    }

    QtObject {
        id: matrixComposerPermissions

        function canSend(_eventType) {
            return true;
        }
    }

    QtObject {
        id: matrixComposerRoom

        property string roomId: roomPreview ? roomPreview.roomid : ""
        property bool isEncrypted: roomPreview ? !!roomPreview.isEncrypted : false
        property int roomMemberCount: roomPreview && roomPreview.roomMemberCount !== undefined
            ? Number(roomPreview.roomMemberCount)
            : 0
        property var permissions: matrixComposerPermissions
        property var input: matrixComposerInputController

        function showEvent(eventId) {
            return rootItem.jumpToLoadedMatrixEvent(eventId);
        }

        function openUserProfile(userId) {
            matrixDialogRoomModel.openUserProfile(userId);
        }
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

    PreviewPermissions {
        id: matrixHeaderPreviewPermissions
    }

    QtObject {
        id: matrixHeaderRoomModel

        property string roomId: roomPreview ? roomPreview.roomid : ""
        property int roomMemberCount: roomPreview && roomPreview.memberCount !== undefined
            ? Number(roomPreview.memberCount)
            : (roomPreview && roomPreview.roomMemberCount !== undefined
                ? Number(roomPreview.roomMemberCount)
                : 0)
        property var pinnedMessages: TimelineManager.matrixTimelinePinnedEventIds
        property var widgetLinks: []
        property bool isEncrypted: !!roomPreview && roomPreview.isEncrypted
        property bool isPublic: !roomPreview || roomPreview.isPublic
        property AbstractPermissions permissions: matrixHeaderPreviewPermissions
        property bool supportsSearch: false
        property bool supportsPinnedMessagesUi: true
        property bool supportsVisibilityInfo: true

        function previewDataForEvent(eventId) {
            const model = TimelineManager.matrixTimelineModel;
            if (!model)
                return ({});

            const row = model.rowForEventId(String(eventId || ""));
            if (row < 0)
                return ({});

            const item = model.itemAt(row);
            if (!item || item.typeString === undefined)
                return ({});

            const previousItem = row > 0 ? model.itemAt(row - 1) : ({});
            const timestamp = Number(item.timestamp || 0);
            const dayKey = rootItem.matrixTimelineDayKey(timestamp);
            const previousTimestamp = previousItem.timestamp !== undefined
                ? new Date(Number(previousItem.timestamp))
                : new Date(timestamp);
            const previousDay = previousItem.timestamp !== undefined
                ? rootItem.matrixTimelineDayKey(previousItem.timestamp)
                : dayKey;
            const previousIsStateEvent = previousItem.eventId === undefined
                ? true
                : rootItem.isMatrixStateLikeKind(previousItem.typeString);
            const previousUserId = previousItem.userId !== undefined
                ? String(previousItem.userId || "")
                : "";
            const itemKind = String(item.typeString || "");
            const body = String(item.body || "");
            const effectiveFileName = item.filename && String(item.filename).length > 0
                ? String(item.filename)
                : (body.length > 0 ? body : qsTr("Attachment"));
            const humanReadableMediaSize = Number(item.filesizeBytes || 0) > 0
                ? Komai.humanReadableFileSize(Number(item.filesizeBytes))
                : "";
            const basePreview = {
                "room": matrixHeaderRoomModel,
                "eventId": String(item.eventId || ""),
                "userId": String(item.userId || ""),
                "userName": String(item.userName || ""),
                "avatarUrl": String(item.senderAvatarUrl || ""),
                "previousDay": previousDay,
                "previousTimestamp": previousTimestamp,
                "previousIsStateEvent": previousIsStateEvent,
                "previousUserId": previousUserId
            };
            const redactedPair = rootItem.matrixRedactedEventPair(item.userName,
                                                                  item.userId);

            if (itemKind === "redacted") {
                return Object.assign({}, basePreview, {
                    "type": MtxEvent.Redacted,
                    "redactedFirst": redactedPair.first,
                    "redactedSecond": redactedPair.second
                });
            }

            if (rootItem.isMatrixStateLikeKind(itemKind)) {
                return Object.assign({}, basePreview, {
                    "type": MtxEvent.Name,
                    "formattedStateEvent": rootItem.formattedMatrixTextHtml(body),
                    "stateEventIconSource": rootItem.matrixStateEventIconForKind(itemKind)
                });
            }

            if (itemKind === "image" || itemKind === "sticker" || itemKind === "video") {
                const mediaWidth = Math.round(Number(item.originalWidth || 0));
                const mediaHeight = Math.round(Number(item.originalHeight || 0));
                const safePreviewAspectRatio = mediaWidth > 0 && mediaHeight > 0
                    ? (mediaHeight / mediaWidth)
                    : 0.75;
                return Object.assign({}, basePreview, {
                    "type": rootItem.matrixEventTypeForItemKind(itemKind),
                    "body": body,
                    "url": String(item.url || ""),
                    "blurhash": "",
                    "filename": effectiveFileName,
                    "filesize": humanReadableMediaSize,
                    "filesizeBytes": Math.round(Number(item.filesizeBytes || 0)),
                    "mimetype": String(item.mimetype || ""),
                    "thumbnailUrl": String(item.thumbnailUrl || ""),
                    "originalWidth": mediaWidth,
                    "originalHeight": mediaHeight,
                    "proportionalHeight": safePreviewAspectRatio,
                    "containerHeight": rootItem.height > 0 ? rootItem.height : Screen.height,
                    "duration": Math.round(Number(item.duration || 0))
                });
            }

            if (itemKind === "file" || itemKind === "audio") {
                return Object.assign({}, basePreview, {
                    "type": rootItem.matrixEventTypeForItemKind(itemKind),
                    "body": body,
                    "filename": effectiveFileName,
                    "filesize": humanReadableMediaSize,
                    "fileTypeIconSource": Komai.fileTypeIconSource(String(item.mimetype || "")),
                    "mimetype": String(item.mimetype || ""),
                    "duration": Math.round(Number(item.duration || 0))
                });
            }

            return Object.assign({}, basePreview, {
                "type": rootItem.matrixEventTypeForItemKind(itemKind),
                "body": body,
                "formattedBody": rootItem.formattedMatrixTextHtml(body),
                "formattedStateEvent": rootItem.formattedMatrixTextHtml(body),
                "stateEventIconSource": rootItem.matrixStateEventIconForKind(itemKind),
                "typeString": itemKind,
                "callType": "",
                "isOnlyEmoji": 0
            });
        }

        function getDump(eventId, _scope) {
            const preview = previewDataForEvent(eventId);
            return {
                "eventId": String(eventId || ""),
                "userId": String((preview && preview.userId) || ""),
                "userName": String((preview && preview.userName) || "")
            };
        }

        function showEvent(eventId) {
            return rootItem.jumpToLoadedMatrixEvent(String(eventId || ""));
        }

        function openUserProfile(userId) {
            matrixDialogRoomModel.openUserProfile(userId);
        }

        function formatRedactedEvent(eventId) {
            const preview = previewDataForEvent(eventId);
            const first = String((preview && preview.redactedFirst) || "");
            const second = String((preview && preview.redactedSecond) || "");
            if (first.length > 0 || second.length > 0) {
                return {
                    "first": first,
                    "second": second
                };
            }

            return rootItem.matrixRedactedEventPair("", "");
        }

        function unpin(eventId) {
            TimelineManager.unpinActiveMatrixTimelineEvent(String(eventId || ""));
        }
    }
}
