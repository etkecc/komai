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

        function setText(value) {
            text = String(value || "");
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
            return support.rootItem.jumpToLoadedMatrixEvent(eventId);
        }

        function openUserProfile(userId) {
            support.dialogRoomModel.openUserProfile(userId);
        }
    }
}
