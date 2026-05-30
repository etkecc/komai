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

        // Intentional mentions (MSC3952) for the *active* room's draft. `mentions`
        // holds identifiers ("@room" or a user MXID) and drives both the warning
        // bar and the `m.mentions` content attached when the message is sent.
        // `mentionTexts` is the parallel list of inserted composer text used to
        // drop a mention when its text is edited away. `dismissedMentions` are
        // ids the user explicitly dismissed via the warning bar (kept out of the
        // ping even though their text remains in the composer).
        //
        // This is per-room draft state living on a shared controller, so it is
        // snapshotted into `_roomMentions` (keyed by room id) when leaving a room
        // and restored when returning — the same save/restore handoff the text
        // draft uses (see MessageInput.onRoomPreviewChanged). The map is
        // in-memory: across an app restart these re-derive from the persisted
        // text draft via deriveMentionsFromText().
        property var mentions: []
        property var mentionTexts: []
        property var dismissedMentions: []
        property var _roomMentions: ({})

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
            refreshMentions(text);
            // A programmatic whole-draft set (e.g. starting an edit, where the
            // body carries existing pills) cannot attribute its pills to a
            // completer pick, so re-derive them. Runs only on these bulk sets,
            // never per keystroke, so a dismissal is not undone mid-typing.
            // (Tab-switch draft restore goes through loadMentionsForRoom.)
            deriveMentionsFromText(text);
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
            refreshMentions(normalized);
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

        function commandExpectsUserIdAt(currentText, cursorPosition) {
            return TimelineManager.activeMatrixCommandExpectsUserIdAt(String(currentText || ""),
                                                                      Number(cursorPosition) || 0);
        }

        // --- Low-level mention list/dismissal mutators -----------------------
        // Arrays are reassigned (not mutated in place) so QML bindings — the
        // warning bar's `input.mentions` — re-evaluate.

        function _setMentionPresent(id, mentionText, present) {
            const idx = mentions.indexOf(id);
            if (present && idx === -1) {
                const nextMentions = mentions.slice();
                const nextTexts = mentionTexts.slice();
                nextMentions.push(id);
                nextTexts.push(String(mentionText || id));
                mentions = nextMentions;
                mentionTexts = nextTexts;
            } else if (!present && idx !== -1) {
                const nextMentions = mentions.slice();
                const nextTexts = mentionTexts.slice();
                nextMentions.splice(idx, 1);
                nextTexts.splice(idx, 1);
                mentions = nextMentions;
                mentionTexts = nextTexts;
            }
        }

        function _markDismissed(id) {
            if (dismissedMentions.indexOf(id) === -1) {
                const next = dismissedMentions.slice();
                next.push(id);
                dismissedMentions = next;
            }
        }

        function _clearDismissed(id) {
            const idx = dismissedMentions.indexOf(id);
            if (idx !== -1) {
                const next = dismissedMentions.slice();
                next.splice(idx, 1);
                dismissedMentions = next;
            }
        }

        // --- Public mention API (called from the composer / warning bar) -----

        // Explicit pick from the autocomplete: this is an affirmative request to
        // mention, so it clears any prior dismissal of that id.
        function addMention(userId, completion) {
            const id = String(userId || "");
            if (id.length === 0)
                return;
            _clearDismissed(id);
            _setMentionPresent(id, completion, true);
        }

        // Warning-bar dismiss ("Don't mention them in this message"): drop the
        // ping but keep the text/pill, and remember the dismissal so re-detection
        // and derivation don't silently resurrect it while the text remains.
        function removeMention(mention) {
            const id = String(mention || "");
            if (id.length === 0)
                return;
            _markDismissed(id);
            _setMentionPresent(id, "", false);
        }

        function detectRoomMention(value) {
            // Match a standalone "@room" token (not "@rooms", "@roommate", etc.).
            return /@room\b/.test(String(value || ""));
        }

        // Re-add user mentions for matrix-link pills present in the text,
        // skipping any the user dismissed. Used on bulk text replacements (edit
        // and draft restore, and paste) where the originating completer pick is
        // gone. The link parsing/validation lives in Rust (Komai.composer
        // ExtractMentions, ruma-backed); here we only apply the dismissal rule
        // and track each match's `source` substring for later pruning.
        function deriveMentionsFromText(value) {
            const matches = Komai.composerExtractMentions(String(value || ""));
            for (let i = 0; i < matches.length; ++i) {
                const id = matches[i].userId;
                if (dismissedMentions.indexOf(id) === -1)
                    _setMentionPresent(id, matches[i].source, true);
            }
        }

        // Reconcile mentions with the current text. Runs on every text change:
        // prunes user mentions whose inserted text was edited away (resetting
        // their dismissal so a later re-insert is treated fresh) and recomputes
        // the @room ping purely from (token present, ping permission, not
        // dismissed). Does NOT derive user pills — that is reserved for bulk
        // restore/paste so a dismissal is never undone mid-typing.
        function refreshMentions(value) {
            const value_ = String(value || "");

            if (mentions.length > 0) {
                let nextMentions = [];
                let nextTexts = [];
                let pruned = false;
                for (let i = 0; i < mentions.length; ++i) {
                    const id = mentions[i];
                    const mentionText = mentionTexts[i];
                    if (id === "@room" || (mentionText && value_.indexOf(mentionText) !== -1)) {
                        nextMentions.push(id);
                        nextTexts.push(mentionText);
                    } else {
                        pruned = true;
                        _clearDismissed(id);
                    }
                }
                if (pruned) {
                    mentions = nextMentions;
                    mentionTexts = nextTexts;
                }
            }

            const hasToken = detectRoomMention(value_);
            if (!hasToken)
                _clearDismissed("@room");
            const canPingRoom = !!(support.permissions
                && typeof support.permissions.canPingRoom === "function"
                && support.permissions.canPingRoom());
            const wantRoom = hasToken && canPingRoom && dismissedMentions.indexOf("@room") === -1;
            _setMentionPresent("@room", "@room", wantRoom);
        }

        // --- Per-room draft state save / restore -----------------------------

        function clearMentionState() {
            mentions = [];
            mentionTexts = [];
            dismissedMentions = [];
        }

        // Snapshot the active room's mention state before leaving it. Empty
        // state drops the entry so the map does not grow unbounded.
        function saveMentionsForRoom(roomId) {
            const id = String(roomId || "");
            if (id.length === 0)
                return;
            if (mentions.length === 0 && dismissedMentions.length === 0) {
                delete _roomMentions[id];
                return;
            }
            _roomMentions[id] = {
                mentions: mentions.slice(),
                mentionTexts: mentionTexts.slice(),
                dismissedMentions: dismissedMentions.slice()
            };
        }

        // Restore the entering room's mention state. If we have a snapshot from
        // earlier this session, apply it verbatim (preserving dismissals);
        // otherwise this is a first visit / post-restart draft, so derive
        // mentions from the restored text.
        function loadMentionsForRoom(roomId, value) {
            const id = String(roomId || "");
            const text = String(value || "");
            clearMentionState();
            const saved = id.length > 0 ? _roomMentions[id] : null;
            if (saved) {
                mentions = saved.mentions.slice();
                mentionTexts = saved.mentionTexts.slice();
                dismissedMentions = saved.dismissedMentions.slice();
                // Re-evaluate @room against the current text/permission; user
                // mentions are kept verbatim from the snapshot.
                refreshMentions(text);
            } else {
                refreshMentions(text);
                deriveMentionsFromText(text);
            }
        }

        function sticker(_row) {
        }
    }

    QtObject {
        id: matrixComposerRoom

        property string roomId: roomPreview ? roomPreview.roomid : ""
        property string roomName: roomPreview ? String(roomPreview.roomName || "") : ""
        property string roomAvatarUrl: roomPreview ? String(roomPreview.roomAvatarUrl || "") : ""
        property bool isActiveMatrixTimelineRoom: true
        property bool isDirect: roomPreview ? !!roomPreview.isDirect : false
        property bool isEncrypted: roomPreview ? !!roomPreview.isEncrypted : false
        property bool isSpace: roomPreview ? !!roomPreview.isSpace : false
        property int roomMemberCount: roomPreview && roomPreview.roomMemberCount !== undefined
            ? Number(roomPreview.roomMemberCount)
            : 0
        property var permissions: support.permissions
        property var input: matrixComposerInputController
        property var typingUsers: TimelineManager.matrixTimelineTypingUsers

        function formatTypingUsers(users, _bg) {
            if (!users || users.length === 0)
                return "";
            if (users.length === 1)
                return qsTr("%1 is typing…").arg(users[0]);
            if (users.length === 2)
                return qsTr("%1 and %2 are typing…").arg(users[0]).arg(users[1]);
            return qsTr("%1, %2 and %3 others are typing…").arg(users[0]).arg(users[1]).arg(users.length - 2);
        }

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
