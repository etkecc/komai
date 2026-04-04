// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import cc.etke.komai

Item {
    id: support

    required property var rootItem
    required property var roomPreview
    required property var dialogRoomModel
    required property var headerRoomModel
    required property AbstractPermissions permissions

    width: 0
    height: 0

    readonly property var uploadsController: matrixUploadsController
    readonly property var composerInputController: matrixComposerInputController
    readonly property var composerRoom: matrixComposerRoom

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

        function refreshCommandInspection(value) {
            if (TimelineManager.matrixTimelineEditEventId.length > 0) {
                commandValidationMessage = "";
                commandValidationState = "none";
                return;
            }

            const inspection = TimelineManager.inspectActiveMatrixSlashCommand(String(value || ""));
            commandValidationMessage = String((inspection && inspection.validationMessage) || "");
            commandValidationState = String((inspection && inspection.validationState) || "none");
        }

        function setText(value) {
            text = String(value || "");
            refreshCommandInspection(text);
        }

        function openFileSelection() {
            return TimelineManager.openActiveMatrixAttachmentSelection();
        }

        function send() {
            return support.rootItem.trySendMessage();
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
            refreshCommandInspection(normalized);
        }

        function clipboardText() {
            return Clipboard.text;
        }

        function tryPasteAttachment(strict) {
            return TimelineManager.tryPasteClipboardAttachment(!!strict);
        }

        function commandCompletionSearchString(prefix, cursorPosition) {
            return TimelineManager.activeMatrixCommandCompletionSearchString(String(prefix || ""),
                                                                            Number(cursorPosition) || 0);
        }

        function applyCommandCompletion(currentText, cursorPosition, completion) {
            return TimelineManager.activeMatrixApplyCommandCompletion(String(currentText || ""),
                                                                      Number(cursorPosition) || 0,
                                                                      String(completion || ""));
        }

        function commandCompletionCursorPosition(currentText, cursorPosition, completion) {
            return TimelineManager.activeMatrixCommandCompletionCursorPosition(
                        String(currentText || ""),
                        Number(cursorPosition) || 0,
                        String(completion || ""));
        }

        function addMention(_userId, _completion) {
        }

        function sticker(_row) {
        }
    }

    QtObject {
        id: matrixComposerRoom

        property string roomId: roomPreview ? roomPreview.roomid : ""
        property bool isActiveMatrixTimelineRoom: true
        property bool isEncrypted: roomPreview ? !!roomPreview.isEncrypted : false
        property int roomMemberCount: roomPreview && roomPreview.roomMemberCount !== undefined
            ? Number(roomPreview.roomMemberCount)
            : 0
        property var permissions: support.permissions
        property var input: matrixComposerInputController

        function showEvent(eventId) {
            return support.rootItem.jumpToLoadedMatrixEvent(eventId);
        }

        function openUserProfile(userId) {
            support.dialogRoomModel.openUserProfile(userId);
        }

        function previewDataForEvent(eventId) {
            if (!support.headerRoomModel
                    || typeof support.headerRoomModel.previewDataForEvent !== "function") {
                return ({});
            }

            return support.headerRoomModel.previewDataForEvent(String(eventId || ""));
        }

        function showImage() {
            return true;
        }
    }
}
