// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../room/components"
import "../../composer" as Composer
import "../../dialogs/timeline" as TimelineDialogs
import "../styles/bubble"
import "../styles/plain"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import cc.etke.komai

ColumnLayout {
    id: root

    required property var roomPreview
    required property bool showBackButton
    property var chatRoot: null
    property var emojiPopup: null
    property var filteredTimeline: null

    readonly property bool hasTimeline: TimelineManager.matrixTimelineItemCount > 0
    readonly property bool loading: TimelineManager.matrixTimelineLoading
    readonly property var composerShell: composerContainer
    readonly property int pendingAttachmentCount: TimelineManager.matrixTimelineAttachmentCount
    readonly property bool hasPendingAttachments: pendingAttachmentCount > 0
    readonly property string activeEditEventId: TimelineManager.matrixTimelineEditEventId
    readonly property bool editing: activeEditEventId.length > 0
    property string draftBeforeEdit: ""
    property bool restoringEditDraft: false
    property int lastPaginationTriggerCount: -1

    function matrixEventTypeForItemKind(kind) {
        switch (kind) {
        case "notice":
            return MtxEvent.NoticeMessage;
        case "image":
            return MtxEvent.ImageMessage;
        case "video":
            return MtxEvent.VideoMessage;
        case "audio":
            return MtxEvent.AudioMessage;
        case "file":
            return MtxEvent.FileMessage;
        case "sticker":
            return MtxEvent.Sticker;
        default:
            return MtxEvent.TextMessage;
        }
    }

    function matrixTimelineDayKey(timestampMs) {
        const day = new Date(Number(timestampMs || 0));
        return day.getFullYear() * 10000 + (day.getMonth() + 1) * 100 + day.getDate();
    }

    function isMatrixStateLikeKind(kind) {
        return ["membership_change", "profile_change", "other_state", "failed_to_parse_state", "date_divider"].indexOf(String(kind || "")) >= 0;
    }

    function formattedMatrixTextHtml(text) {
        return TimelineManager.escapeEmoji(TimelineManager.htmlEscape(String(text || "")).replace(/\n/g, "<br>"));
    }

    function matrixStateEventIconForKind(kind) {
        switch (String(kind || "")) {
        case "membership_change":
            return ":/icons/icons/ui/state-member-join.svg";
        case "profile_change":
            return ":/icons/icons/ui/state-member-display-name.svg";
        default:
            return ":/icons/icons/ui/state-event.svg";
        }
    }

    function openMatrixMessageContextMenu(messageModel, roomModel, copyText) {
        if (!messageModel || !roomModel || !messageModel.eventId)
            return;

        matrixMessageContextMenu.show(messageModel.eventId,
                                      messageModel.threadId || "",
                                      messageModel.type,
                                      !!messageModel.isSender,
                                      !!messageModel.isEncrypted,
                                      !!messageModel.isEditable,
                                      !!messageModel.isStateEvent,
                                      "",
                                      copyText || "",
                                      null,
                                      messageModel,
                                      roomModel);
    }

    function jumpToLoadedMatrixEvent(eventId) {
        const trimmedEventId = String(eventId || "").trim();
        if (trimmedEventId.length === 0 || !TimelineManager.matrixTimelineModel || !matrixTimelineList)
            return false;

        const row = TimelineManager.matrixTimelineModel.rowForEventId(trimmedEventId);
        if (row < 0)
            return false;

        matrixTimelineList.positionViewAtIndex(row, ListView.Center);
        return true;
    }

    function focusTextInput() {
        return composerInput ? composerInput.focusTextInput() : false;
    }

    function destroyOnClose(dialog) {
        if (!dialog)
            return;

        if (root.chatRoot && root.chatRoot.dialogHost && root.chatRoot.dialogHost.destroyOnClose != undefined) {
            root.chatRoot.dialogHost.destroyOnClose(dialog);
            return;
        }

        if (dialog.closing != undefined)
            dialog.closing.connect(() => dialog.destroy(1000));
        else if (dialog.aboutToHide != undefined)
            dialog.aboutToHide.connect(() => dialog.destroy(1000));
    }

    function showDialogFromComponent(componentRef, properties) {
        const dialogParent = root.chatRoot && root.chatRoot.dialogHost
            ? root.chatRoot.dialogHost
            : (root.chatRoot ? root.chatRoot : root);
        const dialog = componentRef.createObject(dialogParent, properties || {});
        if (!dialog)
            return null;
        dialog.open();
        root.destroyOnClose(dialog);
        return dialog;
    }

    function openRemoveMessageDialog(eventId) {
        const trimmedEventId = String(eventId || "").trim();
        if (trimmedEventId.length === 0)
            return null;

        return showDialogFromComponent(removeReasonDialogComponent, {
                "eventId": trimmedEventId
            });
    }

    function openRawMessageDialog(eventId) {
        const trimmedEventId = String(eventId || "").trim();
        if (trimmedEventId.length === 0)
            return null;

        const payload = TimelineManager.rawMessageDialogForActiveMatrixTimelineEvent(trimmedEventId);
        if (!payload || !payload.rawMessageJson)
            return null;

        return showDialogFromComponent(rawMessageDialogComponent, payload);
    }

    function openReadReceiptsDialog(eventId) {
        const trimmedEventId = String(eventId || "").trim();
        if (trimmedEventId.length === 0)
            return null;

        const readReceipts = TimelineManager.readReceiptsModelForActiveMatrixTimelineEvent(trimmedEventId);
        if (!readReceipts)
            return null;

        return showDialogFromComponent(readReceiptsDialogComponent, {
                "readReceipts": readReceipts,
                "room": matrixDialogRoomModel
            });
    }

    function appendText(text) {
        return composerInput ? composerInput.appendText(text) : false;
    }

    function trySendMessage() {
        if (root.hasPendingAttachments)
            return TimelineManager.sendActiveMatrixAttachments();

        const body = composerInput.text;
        const ok = root.editing
            ? TimelineManager.sendActiveMatrixEditMessage(body)
            : TimelineManager.sendActiveMatrixTextMessage(body);
        if (!ok)
            return false;

        if (!root.editing)
            matrixComposerInputController.setText("");
        root.focusTextInput();
        return true;
    }

    function beginEdit(eventId, body, messageKind) {
        if (!eventId || !body)
            return false;

        if (!root.editing) {
            draftBeforeEdit = composerInput.text;
            restoringEditDraft = true;
        }

        if (!TimelineManager.queueActiveMatrixEdit(String(eventId), String(body), String(messageKind || "message"))) {
            if (restoringEditDraft) {
                draftBeforeEdit = "";
                restoringEditDraft = false;
            }
            return false;
        }

        matrixComposerInputController.setText(String(body));
        root.focusTextInput();
        return true;
    }

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
            return root.trySendMessage();
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

        property string roomId: root.roomPreview ? root.roomPreview.roomid : ""
        property bool isEncrypted: root.roomPreview ? !!root.roomPreview.isEncrypted : false
        property var permissions: matrixComposerPermissions
        property var input: matrixComposerInputController
    }

    QtObject {
        id: matrixMessageActionsDefaultPermissions

        function canSend(eventType) {
            return false;
        }

        function canRedact() {
            return false;
        }

        function canChange(eventType) {
            return false;
        }
    }

    QtObject {
        id: matrixMessageActionsDefaultRoomModel

        property string roomId: root.roomPreview ? root.roomPreview.roomid : ""
        property var permissions: matrixMessageActionsDefaultPermissions
        property var input: null
        property var frequentReactions: []

        function showEvent(eventId) {
            return root.jumpToLoadedMatrixEvent(eventId);
        }
    }

    MessageContextMenu {
        id: matrixMessageContextMenu

        chatRoot: root
        emojiPopup: root.emojiPopup
        filteredTimelineModel: root.filteredTimeline
    }

    ReplyContextMenu {
        id: matrixReplyContextMenu

        roomModel: matrixMessageActionsDefaultRoomModel
    }

    MessageActionsHost {
        id: matrixMessageActionsHost

        chatList: matrixTimelineList
        chatRoot: root
        emojiPopup: root.emojiPopup
        filteredTimeline: root.filteredTimeline
        roomModel: matrixMessageActionsDefaultRoomModel
    }

    Component {
        id: removeReasonDialogComponent

        InputDialog {
            required property string eventId

            placeholderText: qsTr("Optional reason")
            title: qsTr("Delete this message?")
            titleIcon: ":/icons/icons/ui/delete.svg"
            acceptText: qsTr("Delete")

            onInputAccepted: function (text) {
                TimelineManager.redactActiveMatrixTimelineEvent(eventId, text);
            }
        }
    }

    Component {
        id: rawMessageDialogComponent

        TimelineDialogs.RawMessageDialog {
        }
    }

    Component {
        id: readReceiptsDialogComponent

        TimelineDialogs.ReadReceipts {
        }
    }

    QtObject {
        id: matrixDialogRoomModel

        property string roomId: root.roomPreview ? root.roomPreview.roomid : ""

        function openUserProfile(userId) {
            const trimmedUserId = String(userId || "").trim();
            if (trimmedUserId.length === 0)
                return;

            TimelineManager.openGlobalUserProfile(trimmedUserId);
        }
    }

    PreviewPermissions {
        id: matrixHeaderPreviewPermissions
    }

    QtObject {
        id: matrixHeaderRoomModel

        property string roomId: root.roomPreview ? root.roomPreview.roomid : ""
        property int roomMemberCount: root.roomPreview && root.roomPreview.memberCount !== undefined
            ? Number(root.roomPreview.memberCount)
            : (root.roomPreview && root.roomPreview.roomMemberCount !== undefined
                ? Number(root.roomPreview.roomMemberCount)
                : 0)
        property var pinnedMessages: TimelineManager.matrixTimelinePinnedEventIds
        property var widgetLinks: []
        property bool isEncrypted: !!root.roomPreview && root.roomPreview.isEncrypted
        property AbstractPermissions permissions: matrixHeaderPreviewPermissions
        property bool supportsSearch: false
        property bool supportsPinnedMessagesUi: true
        property bool supportsVisibilityInfo: false

        function previewDataForEvent(eventId) {
            const model = TimelineManager.matrixTimelineModel;
            if (!model)
                return ({});

            const row = model.rowForEventId(String(eventId || ""));
            if (row < 0)
                return ({});

            const item = model.itemAt(row);
            if (!item || item.itemKind === undefined)
                return ({});

            const previousItem = model.itemAt(row + 1);
            const timestamp = Number(item.timestamp || 0);
            const dayKey = root.matrixTimelineDayKey(timestamp);
            const previousTimestamp = previousItem.timestamp !== undefined
                ? new Date(Number(previousItem.timestamp))
                : new Date(timestamp);
            const previousDay = previousItem.timestamp !== undefined
                ? root.matrixTimelineDayKey(previousItem.timestamp)
                : dayKey;
            const previousIsStateEvent = previousItem.eventId === undefined
                ? true
                : root.isMatrixStateLikeKind(previousItem.itemKind);
            const previousUserId = previousItem.senderId !== undefined
                ? String(previousItem.senderId || "")
                : "";
            const itemKind = String(item.itemKind || "");
            const body = String(item.body || "");
            const effectiveFileName = item.fileName && String(item.fileName).length > 0
                ? String(item.fileName)
                : (body.length > 0 ? body : qsTr("Attachment"));
            const humanReadableMediaSize = Number(item.mediaSizeBytes || 0) > 0
                ? Komai.humanReadableFileSize(Number(item.mediaSizeBytes))
                : "";
            const basePreview = {
                "room": matrixHeaderRoomModel,
                "eventId": String(item.eventId || ""),
                "userId": String(item.senderId || ""),
                "userName": String(item.senderDisplayName || ""),
                "avatarUrl": String(item.senderAvatarUrl || ""),
                "previousDay": previousDay,
                "previousTimestamp": previousTimestamp,
                "previousIsStateEvent": previousIsStateEvent,
                "previousUserId": previousUserId
            };

            if (root.isMatrixStateLikeKind(itemKind)) {
                return Object.assign({}, basePreview, {
                    "type": MtxEvent.Name,
                    "formattedStateEvent": root.formattedMatrixTextHtml(body),
                    "stateEventIconSource": root.matrixStateEventIconForKind(itemKind)
                });
            }

            if (itemKind === "image" || itemKind === "sticker" || itemKind === "video") {
                const mediaWidth = Math.round(Number(item.mediaWidth || 0));
                const mediaHeight = Math.round(Number(item.mediaHeight || 0));
                const safePreviewAspectRatio = mediaWidth > 0 && mediaHeight > 0
                    ? (mediaHeight / mediaWidth)
                    : 0.75;
                return Object.assign({}, basePreview, {
                    "type": root.matrixEventTypeForItemKind(itemKind),
                    "body": body,
                    "url": String(item.mediaUrl || ""),
                    "blurhash": "",
                    "filename": effectiveFileName,
                    "filesize": humanReadableMediaSize,
                    "filesizeBytes": Math.round(Number(item.mediaSizeBytes || 0)),
                    "mimetype": String(item.mimeType || ""),
                    "thumbnailUrl": String(item.thumbnailUrl || ""),
                    "originalWidth": mediaWidth,
                    "originalHeight": mediaHeight,
                    "proportionalHeight": safePreviewAspectRatio,
                    "containerHeight": root.height > 0 ? root.height : Screen.height,
                    "duration": Math.round(Number(item.mediaDurationMs || 0))
                });
            }

            if (itemKind === "file" || itemKind === "audio") {
                return Object.assign({}, basePreview, {
                    "type": root.matrixEventTypeForItemKind(itemKind),
                    "body": body,
                    "filename": effectiveFileName,
                    "filesize": humanReadableMediaSize,
                    "fileTypeIconSource": Komai.fileTypeIconSource(String(item.mimeType || "")),
                    "mimetype": String(item.mimeType || ""),
                    "duration": Math.round(Number(item.mediaDurationMs || 0))
                });
            }

            return Object.assign({}, basePreview, {
                "type": root.matrixEventTypeForItemKind(itemKind),
                "body": body,
                "formattedBody": root.formattedMatrixTextHtml(body),
                "formattedStateEvent": root.formattedMatrixTextHtml(body),
                "stateEventIconSource": root.matrixStateEventIconForKind(itemKind),
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
            return root.jumpToLoadedMatrixEvent(String(eventId || ""));
        }

        function openUserProfile(userId) {
            matrixDialogRoomModel.openUserProfile(userId);
        }

        function unpin(eventId) {
            TimelineManager.unpinActiveMatrixTimelineEvent(String(eventId || ""));
        }
    }

    anchors.fill: parent
    enabled: visible
    spacing: 0
    visible: !!roomPreview && roomPreview.isMatrixSummary

    RoomHeader {
        Layout.fillWidth: true
        room: null
        roomModel: matrixHeaderRoomModel
        roomId: root.roomPreview ? root.roomPreview.roomid : ""
        roomName: root.roomPreview ? root.roomPreview.roomName : qsTr("No room selected")
        avatarDisplayName: root.roomPreview ? root.roomPreview.roomName : qsTr("No room selected")
        avatarUrl: root.roomPreview ? root.roomPreview.roomAvatarUrl : ""
        directChatOtherUserId: root.roomPreview ? root.roomPreview.directChatOtherUserId : ""
        isDirect: !!root.roomPreview && root.roomPreview.isDirect
        isEncrypted: !!root.roomPreview && root.roomPreview.isEncrypted
        roomTopic: root.roomPreview ? root.roomPreview.roomTopic : ""
        showBackButton: root.showBackButton
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 1
        color: palette.mid
    }

    Rectangle {
        Layout.fillHeight: true
        Layout.fillWidth: true
        color: palette.base

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            Item {
                Layout.fillHeight: true
                Layout.fillWidth: true

                ListView {
                    id: matrixTimelineList

                    property int delegateMaxWidth: width

                    anchors.fill: parent
                    anchors.margins: Komai.paddingLarge
                    clip: true
                    model: TimelineManager.matrixTimelineModel
                    spacing: Komai.paddingMedium
                    visible: root.hasTimeline

                    onAtYBeginningChanged: {
                        if (atYBeginning
                                && root.hasTimeline
                                && !root.loading
                                && root.lastPaginationTriggerCount !== TimelineManager.matrixTimelineItemCount) {
                            if (TimelineManager.paginateActiveMatrixTimelineBackwards(0))
                                root.lastPaginationTriggerCount = TimelineManager.matrixTimelineItemCount;
                        }
                    }
                    onContentYChanged: {
                        if (contentY > 0 && root.lastPaginationTriggerCount === TimelineManager.matrixTimelineItemCount)
                            root.lastPaginationTriggerCount = -1;
                    }

                    delegate: Item {
                        id: timelineItemDelegate

                        property var chat: matrixTimelineList
                        property var chatRoot: root.chatRoot ? root.chatRoot : matrixTimelineList

                        required property string itemKind
                        required property string itemId
                        required property string eventId
                        required property string senderDisplayName
                        required property string senderAvatarUrl
                        required property string senderId
                        required property string body
                        required property string replyEventId
                        required property string replySenderDisplayName
                        required property string replyBody
                        required property var reactions
                        required property string reactionsSummary
                        required property string fileName
                        required property string mimeType
                        required property string mediaUrl
                        required property string thumbnailUrl
                        required property double mediaWidth
                        required property double mediaHeight
                        required property double mediaDurationMs
                        required property double mediaSizeBytes
                        required property bool mediaIsEncrypted
                        required property bool thumbnailIsEncrypted
                        required property double timestamp
                        required property bool isEdited
                        required property bool isOwn

                        readonly property int modelIndex: TimelineManager.matrixTimelineModel
                            ? TimelineManager.matrixTimelineModel.rowForEventId(eventId)
                            : -1
                        readonly property bool isMediaItem: ["image", "video", "audio", "file", "sticker"].indexOf(itemKind) >= 0
                        readonly property string effectiveFileName: fileName.length > 0 ? fileName : (body.length > 0 ? body : qsTr("Attachment"))
                        readonly property string replySourceBody: body.length > 0 ? body : effectiveFileName
                        readonly property double safePreviewAspectRatio: mediaWidth > 0 && mediaHeight > 0 ? (mediaHeight / mediaWidth) : 0.75
                        readonly property bool isStateLikeItem: ["membership_change", "profile_change", "other_state", "failed_to_parse_state"].indexOf(itemKind) >= 0
                        readonly property bool usesSharedImageBubble: itemKind === "image"
                        readonly property bool usesSharedStickerBubble: itemKind === "sticker"
                        readonly property bool usesSharedVideoBubble: itemKind === "video"
                        readonly property bool usesSharedFileBubble: itemKind === "file"
                        readonly property bool usesSharedAudioBubble: itemKind === "audio"
                        readonly property bool usesSharedStateBubble: isStateLikeItem
                        readonly property bool usesSharedTextBubble: itemKind !== "date_divider"
                            && !isStateLikeItem
                            && !isMediaItem
                        readonly property bool usesSharedTimelineBubble: usesSharedTextBubble
                            || usesSharedImageBubble
                            || usesSharedStickerBubble
                            || usesSharedVideoBubble
                            || usesSharedFileBubble
                            || usesSharedAudioBubble
                            || usesSharedStateBubble
                        readonly property bool supportsSharedToolbarActions: eventId.length > 0 && itemKind !== "date_divider" && !isStateLikeItem
                        readonly property int matrixEventType: root.matrixEventTypeForItemKind(itemKind)
                        readonly property int dayKey: root.matrixTimelineDayKey(timestamp)
                        readonly property var previousItem: TimelineManager.matrixTimelineModel ? TimelineManager.matrixTimelineModel.itemAt(modelIndex + 1) : ({})
                        readonly property string sharedHumanReadableMediaSize: mediaSizeBytes > 0
                            ? Komai.humanReadableFileSize(Number(mediaSizeBytes))
                            : ""
                        readonly property string sharedFileTypeIconSource: Komai.fileTypeIconSource(mimeType)
                        readonly property var sharedPreviewData: ({
                                "room": matrixToolbarRoomModel,
                                "avatarUrl": senderAvatarUrl,
                                "body": body,
                                "formattedBody": root.formattedMatrixTextHtml(body),
                                "isOnlyEmoji": 0,
                                "previousDay": previousItem.timestamp !== undefined ? root.matrixTimelineDayKey(previousItem.timestamp) : dayKey,
                                "previousTimestamp": previousItem.timestamp !== undefined ? new Date(Number(previousItem.timestamp)) : new Date(Number(timestamp)),
                                "previousIsStateEvent": previousItem.eventId === undefined ? true : root.isMatrixStateLikeKind(previousItem.itemKind),
                                "previousUserId": previousItem.senderId !== undefined ? String(previousItem.senderId || "") : ""
                            })
                        readonly property var sharedAttachmentPreviewData: ({
                                "room": matrixToolbarRoomModel,
                                "avatarUrl": senderAvatarUrl,
                                "eventId": itemId,
                                "body": body,
                                "filename": effectiveFileName,
                                "filesize": sharedHumanReadableMediaSize,
                                "fileTypeIconSource": sharedFileTypeIconSource,
                                "mimetype": mimeType,
                                "duration": Math.round(Number(mediaDurationMs)),
                                "previousDay": previousItem.timestamp !== undefined ? root.matrixTimelineDayKey(previousItem.timestamp) : dayKey,
                                "previousTimestamp": previousItem.timestamp !== undefined ? new Date(Number(previousItem.timestamp)) : new Date(Number(timestamp)),
                                "previousIsStateEvent": previousItem.eventId === undefined ? true : root.isMatrixStateLikeKind(previousItem.itemKind),
                                "previousUserId": previousItem.senderId !== undefined ? String(previousItem.senderId || "") : ""
                            })
                        readonly property var sharedVisualPreviewData: ({
                                "room": matrixToolbarRoomModel,
                                "avatarUrl": senderAvatarUrl,
                                "url": mediaUrl,
                                "blurhash": "",
                                "eventId": itemId,
                                "body": body,
                                "filename": effectiveFileName,
                                "filesize": sharedHumanReadableMediaSize,
                                "filesizeBytes": Math.round(Number(mediaSizeBytes)),
                                "mimetype": mimeType,
                                "thumbnailUrl": thumbnailUrl,
                                "originalWidth": Math.round(Number(mediaWidth)),
                                "originalHeight": Math.round(Number(mediaHeight)),
                                "proportionalHeight": safePreviewAspectRatio,
                                "containerHeight": matrixTimelineList.height > 0 ? matrixTimelineList.height : root.height,
                                "previousDay": previousItem.timestamp !== undefined ? root.matrixTimelineDayKey(previousItem.timestamp) : dayKey,
                                "previousTimestamp": previousItem.timestamp !== undefined ? new Date(Number(previousItem.timestamp)) : new Date(Number(timestamp)),
                                "previousIsStateEvent": previousItem.eventId === undefined ? true : root.isMatrixStateLikeKind(previousItem.itemKind),
                                "previousUserId": previousItem.senderId !== undefined ? String(previousItem.senderId || "") : ""
                            })
                        readonly property var sharedReplyPreviewData: replyEventId.length > 0
                            ? ({
                                "type": MtxEvent.TextMessage,
                                "body": replyBody,
                                "formattedBody": root.formattedMatrixTextHtml(replyBody),
                                "isOnlyEmoji": 0,
                                "userId": "",
                                "userName": replySenderDisplayName.length > 0 ? replySenderDisplayName : qsTr("Reply")
                            })
                            : ({})
                        readonly property var sharedStatePreviewData: ({
                                "room": matrixToolbarRoomModel,
                                "avatarUrl": senderAvatarUrl,
                                "formattedStateEvent": root.formattedMatrixTextHtml(body),
                                "stateEventIconSource": root.matrixStateEventIconForKind(itemKind),
                                "previousDay": previousItem.timestamp !== undefined ? root.matrixTimelineDayKey(previousItem.timestamp) : dayKey,
                                "previousTimestamp": previousItem.timestamp !== undefined ? new Date(Number(previousItem.timestamp)) : new Date(Number(timestamp)),
                                "previousIsStateEvent": previousItem.eventId === undefined ? true : root.isMatrixStateLikeKind(previousItem.itemKind),
                                "previousUserId": previousItem.senderId !== undefined ? String(previousItem.senderId || "") : ""
                            })
                        width: matrixTimelineList.width
                        height: itemKind === "date_divider"
                            ? dateDivider.implicitHeight
                            : (sharedTimelineBubble.item ? sharedTimelineBubble.item.height : 0)

                        PreviewPermissions {
                            id: matrixToolbarPreviewPermissions
                        }

                        QtObject {
                            id: matrixToolbarInput

                            function reaction(targetEventId, reactionKey) {
                                TimelineManager.toggleActiveMatrixTimelineReaction(
                                    String(targetEventId || timelineItemDelegate.eventId || ""),
                                    String(reactionKey || ""));
                            }
                        }

                        QtObject {
                            id: matrixToolbarRoomModel

                            property string roomId: root.roomPreview ? root.roomPreview.roomid : ""
                            property bool isActiveMatrixTimelineRoom: true
                            property int roomMemberCount: root.roomPreview && root.roomPreview.roomMemberCount !== undefined
                                ? Number(root.roomPreview.roomMemberCount)
                                : 0
                            property bool isEncrypted: root.roomPreview ? !!root.roomPreview.isEncrypted : false
                            property AbstractPermissions permissions: matrixToolbarPreviewPermissions
                            property var input: matrixToolbarInput
                            property var frequentReactions: []
                            property var pinnedMessages: TimelineManager.matrixTimelinePinnedEventIds
                            property string reply: ""
                            property string edit: ""
                            property string thread: ""

                            function formatDateSeparator(timestamp) {
                                return Qt.formatDate(timestamp, "ddd, MMM d");
                            }

                            function formatLaterSeparator(_previous, currentTimestamp) {
                                return Qt.formatTime(currentTimestamp, "hh:mm");
                            }

                            function openUserProfile(userId) {
                                matrixDialogRoomModel.openUserProfile(userId);
                            }

                            function eventShown() {
                            }

                            function openMedia(targetEventId) {
                                const targetItemId = String(targetEventId || timelineItemDelegate.itemId || "");
                                if (targetItemId.length === 0)
                                    return;

                                TimelineManager.openActiveMatrixTimelineMedia(
                                    targetItemId,
                                    timelineItemDelegate.effectiveFileName);
                            }

                            function saveMedia(targetEventId) {
                                const targetItemId = String(targetEventId || timelineItemDelegate.itemId || "");
                                if (targetItemId.length === 0)
                                    return;

                                TimelineManager.saveActiveMatrixTimelineMedia(
                                    targetItemId,
                                    timelineItemDelegate.effectiveFileName);
                            }

                            function showImage() {
                                return true;
                            }

                            onReplyChanged: {
                                if (!reply)
                                    return;

                                TimelineManager.queueActiveMatrixReply(
                                    reply,
                                    timelineItemDelegate.senderDisplayName,
                                    timelineItemDelegate.replySourceBody);
                                reply = "";
                            }
                            onEditChanged: {
                                if (!edit)
                                    return;

                                root.beginEdit(edit,
                                               timelineItemDelegate.body,
                                               timelineItemDelegate.itemKind);
                                edit = "";
                            }
                            onThreadChanged: {
                                if (thread)
                                    thread = "";
                            }

                            function showEvent(eventId) {
                                return root.jumpToLoadedMatrixEvent(eventId);
                            }

                            function copyLinkToEvent(eventId) {
                                TimelineManager.copyMatrixEventLink(
                                    roomId,
                                    String(eventId || timelineItemDelegate.eventId || ""));
                            }

                            function markEventAsRead(eventId) {
                                TimelineManager.markActiveMatrixTimelineEventAsRead(
                                    String(eventId || timelineItemDelegate.eventId || ""));
                            }

                            function pin(eventId) {
                                TimelineManager.pinActiveMatrixTimelineEvent(
                                    String(eventId || timelineItemDelegate.eventId || ""));
                            }

                            function unpin(eventId) {
                                TimelineManager.unpinActiveMatrixTimelineEvent(
                                    String(eventId || timelineItemDelegate.eventId || ""));
                            }

                            function reportEvent(eventId, reason, score) {
                                TimelineManager.reportActiveMatrixTimelineEvent(
                                    String(eventId || timelineItemDelegate.eventId || ""),
                                    String(reason || ""),
                                    Number(score || -50));
                            }

                            function viewRawMessage(eventId) {
                                root.openRawMessageDialog(
                                    String(eventId || timelineItemDelegate.eventId || ""));
                            }

                            function viewDecryptedRawMessage(eventId) {
                                viewRawMessage(eventId);
                            }

                            function showReadReceipts(eventId) {
                                root.openReadReceiptsDialog(
                                    String(eventId || timelineItemDelegate.eventId || ""));
                            }
                        }

                        QtObject {
                            id: matrixToolbarMessageModel

                            readonly property string eventId: timelineItemDelegate.eventId
                            readonly property string threadId: ""
                            readonly property int type: timelineItemDelegate.matrixEventType
                            readonly property bool isSender: timelineItemDelegate.isOwn
                            readonly property bool isEncrypted: timelineItemDelegate.mediaIsEncrypted || timelineItemDelegate.thumbnailIsEncrypted || timelineItemDelegate.itemKind === "unable_to_decrypt"
                            readonly property bool isEditable: !root.hasPendingAttachments
                                && !TimelineManager.matrixTimelineAttachmentSending
                                && timelineItemDelegate.isOwn
                                && ["message", "notice", "emote"].indexOf(timelineItemDelegate.itemKind) >= 0
                            readonly property bool isStateEvent: timelineItemDelegate.isStateLikeItem
                            readonly property string body: timelineItemDelegate.body
                            readonly property bool supportsReaction: timelineItemDelegate.supportsSharedToolbarActions
                            readonly property bool supportsReply: timelineItemDelegate.supportsSharedToolbarActions
                            readonly property bool supportsThread: false
                            readonly property bool supportsForward: false
                            readonly property bool supportsGoToMessage: false
                            readonly property bool supportsOptions: false
                            readonly property bool supportsEdit: isEditable
                            readonly property bool supportsRemove: eventId.length > 0
                                && (TimelineManager.matrixTimelineCanRedactOther
                                    || (timelineItemDelegate.isOwn
                                        && TimelineManager.matrixTimelineCanRedactOwn))
                            readonly property bool supportsViewRaw: eventId.length > 0
                            readonly property bool supportsReadReceipts: timelineItemDelegate.isOwn
                                && timelineItemDelegate.supportsSharedToolbarActions
                            readonly property bool supportsMarkAsRead: timelineItemDelegate.supportsSharedToolbarActions
                            readonly property bool supportsPin: timelineItemDelegate.supportsSharedToolbarActions
                            readonly property bool supportsReport: timelineItemDelegate.supportsSharedToolbarActions
                            readonly property bool supportsOpenMedia: timelineItemDelegate.isMediaItem
                            readonly property bool supportsSaveMedia: timelineItemDelegate.isMediaItem
                            readonly property bool supportsCopyEventLink: eventId.length > 0
                        }

                        Rectangle {
                            id: dateDivider

                            anchors.horizontalCenter: parent.horizontalCenter
                            color: palette.mid
                            height: dividerLabel.implicitHeight + Komai.paddingSmall * 2
                            radius: height / 2
                            visible: itemKind === "date_divider"
                            width: dividerLabel.implicitWidth + Komai.paddingLarge * 2

                            MatrixText {
                                id: dividerLabel

                                anchors.centerIn: parent
                                color: palette.base
                                text: Qt.formatDateTime(new Date(timestamp), "dddd, d MMMM")
                                textFormat: TextEdit.PlainText
                            }
                        }

                        Component {
                            id: matrixPlainMessageStyle

                            TimelinePlainMessageStyle {
                                eventId: timelineItemDelegate.eventId
                                replyTo: !timelineItemDelegate.usesSharedStateBubble
                                    ? timelineItemDelegate.replyEventId
                                    : ""
                                room: null
                                index: timelineItemDelegate.modelIndex
                                day: timelineItemDelegate.dayKey
                                isSender: timelineItemDelegate.isOwn
                                isStateEvent: timelineItemDelegate.usesSharedStateBubble
                                timestamp: new Date(Number(timelineItemDelegate.timestamp))
                                userId: timelineItemDelegate.senderId
                                userName: timelineItemDelegate.senderDisplayName
                                threadId: ""
                                userPowerlevel: 0
                                isEdited: timelineItemDelegate.isEdited
                                isEncrypted: timelineItemDelegate.mediaIsEncrypted
                                    || timelineItemDelegate.thumbnailIsEncrypted
                                reactions: timelineItemDelegate.usesSharedStateBubble
                                    ? []
                                    : timelineItemDelegate.reactions
                                status: MtxEvent.Empty
                                trustlevel: 0
                                notificationlevel: MtxEvent.Empty
                                type: timelineItemDelegate.usesSharedStateBubble
                                    ? MtxEvent.Name
                                    : timelineItemDelegate.matrixEventType
                                isEditable: timelineItemDelegate.usesSharedTextBubble
                                    && matrixToolbarMessageModel.isEditable
                                isHiddenEvent: false
                                messageContextMenu: matrixMessageContextMenu
                                replyContextMenu: matrixReplyContextMenu
                                messageActions: matrixMessageActionsHost.control
                                previewData: timelineItemDelegate.usesSharedStateBubble
                                    ? timelineItemDelegate.sharedStatePreviewData
                                    : (timelineItemDelegate.usesSharedImageBubble
                                        || timelineItemDelegate.usesSharedStickerBubble
                                        || timelineItemDelegate.usesSharedVideoBubble)
                                        ? timelineItemDelegate.sharedVisualPreviewData
                                        : (timelineItemDelegate.usesSharedFileBubble || timelineItemDelegate.usesSharedAudioBubble)
                                        ? timelineItemDelegate.sharedAttachmentPreviewData
                                        : timelineItemDelegate.sharedPreviewData
                                replyPreviewData: !timelineItemDelegate.usesSharedStateBubble
                                    ? timelineItemDelegate.sharedReplyPreviewData
                                    : ({})
                                roomModelOverride: matrixToolbarRoomModel
                                scrolledToThis: false
                            }
                        }

                        Component {
                            id: matrixBubbleMessageStyle

                            TimelineBubbleMessageStyle {
                                eventId: timelineItemDelegate.eventId
                                replyTo: !timelineItemDelegate.usesSharedStateBubble
                                    ? timelineItemDelegate.replyEventId
                                    : ""
                                room: null
                                index: timelineItemDelegate.modelIndex
                                day: timelineItemDelegate.dayKey
                                isSender: timelineItemDelegate.isOwn
                                isStateEvent: timelineItemDelegate.usesSharedStateBubble
                                timestamp: new Date(Number(timelineItemDelegate.timestamp))
                                userId: timelineItemDelegate.senderId
                                userName: timelineItemDelegate.senderDisplayName
                                threadId: ""
                                userPowerlevel: 0
                                isEdited: timelineItemDelegate.isEdited
                                isEncrypted: timelineItemDelegate.mediaIsEncrypted
                                    || timelineItemDelegate.thumbnailIsEncrypted
                                reactions: timelineItemDelegate.usesSharedStateBubble
                                    ? []
                                    : timelineItemDelegate.reactions
                                status: MtxEvent.Empty
                                trustlevel: 0
                                notificationlevel: MtxEvent.Empty
                                type: timelineItemDelegate.usesSharedStateBubble
                                    ? MtxEvent.Name
                                    : timelineItemDelegate.matrixEventType
                                isEditable: timelineItemDelegate.usesSharedTextBubble
                                    && matrixToolbarMessageModel.isEditable
                                isHiddenEvent: false
                                messageContextMenu: matrixMessageContextMenu
                                replyContextMenu: matrixReplyContextMenu
                                messageActions: matrixMessageActionsHost.control
                                previewData: timelineItemDelegate.usesSharedStateBubble
                                    ? timelineItemDelegate.sharedStatePreviewData
                                    : (timelineItemDelegate.usesSharedImageBubble
                                        || timelineItemDelegate.usesSharedStickerBubble
                                        || timelineItemDelegate.usesSharedVideoBubble)
                                        ? timelineItemDelegate.sharedVisualPreviewData
                                        : (timelineItemDelegate.usesSharedFileBubble || timelineItemDelegate.usesSharedAudioBubble)
                                        ? timelineItemDelegate.sharedAttachmentPreviewData
                                        : timelineItemDelegate.sharedPreviewData
                                replyPreviewData: !timelineItemDelegate.usesSharedStateBubble
                                    ? timelineItemDelegate.sharedReplyPreviewData
                                    : ({})
                                roomModelOverride: matrixToolbarRoomModel
                                scrolledToThis: false
                            }
                        }

                        Loader {
                            id: sharedTimelineBubble

                            active: timelineItemDelegate.usesSharedTimelineBubble
                            sourceComponent: Settings.timelineMessagesStyle === Settings.TimelineMessagesStyle.Plain
                                ? matrixPlainMessageStyle
                                : matrixBubbleMessageStyle
                            visible: active
                        }
                    }
                }

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: Komai.paddingMedium
                    visible: !root.hasTimeline
                    width: Math.min(parent.width - Komai.paddingLarge * 2, 560)

                    MatrixText {
                        Layout.fillWidth: true
                        horizontalAlignment: TextEdit.AlignHCenter
                        text: root.loading
                            ? qsTr("Loading room timeline…")
                            : qsTr("No timeline items are loaded for this room yet.")
                        wrapMode: Text.WordWrap
                    }

                    MatrixText {
                        Layout.fillWidth: true
                        color: palette.buttonText
                        horizontalAlignment: TextEdit.AlignHCenter
                        text: qsTr("This room is now backed by the Rust matrix-sdk timeline and shared Komai composer surface while the remaining gaps are migrated.")
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Rectangle {
                id: composerContainer

                Layout.fillWidth: true
                Layout.minimumHeight: implicitHeight
                Layout.preferredHeight: implicitHeight
                Layout.maximumHeight: implicitHeight
                color: palette.window
                implicitHeight: composerLayout.implicitHeight + Komai.paddingMedium * 2

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    color: palette.mid
                    height: 1
                }

                ColumnLayout {
                    id: composerLayout

                    anchors.fill: parent
                    anchors.margins: Komai.paddingMedium
                    spacing: Komai.paddingSmall

                    Composer.UploadBox {
                        Layout.minimumHeight: 0
                        Layout.preferredHeight: layoutVisible ? implicitHeight : 0
                        Layout.maximumHeight: layoutVisible ? implicitHeight : 0
                        uploadsController: matrixUploadsController
                        uploadsSending: TimelineManager.matrixTimelineAttachmentSending
                    }

                    Composer.ReplyPopup {
                        Layout.minimumHeight: 0
                        Layout.preferredHeight: layoutVisible ? implicitHeight : 0
                        Layout.maximumHeight: layoutVisible ? implicitHeight : 0
                        matrixReplyEventId: TimelineManager.matrixTimelineReplyEventId
                        matrixReplyDisplayName: TimelineManager.matrixTimelineReplySenderDisplayName
                        matrixReplyBody: TimelineManager.matrixTimelineReplyBody
                        matrixEditEventId: TimelineManager.matrixTimelineEditEventId
                        roundTopCorners: true
                    }

                    Composer.MessageInput {
                        id: composerInput

                        Layout.fillWidth: true
                        room: matrixComposerRoom
                        timelineRoot: root.chatRoot ? root.chatRoot : root
                        selectionModeRoot: root.chatRoot ? root.chatRoot : matrixTimelineList
                        inputController: matrixComposerInputController
                        allowCalls: false
                        allowStickers: false
                        allowCommandCompleter: false
                        attachmentsEnabled: !root.editing
                        showAllButtons: true
                    }
                }
            }
        }
    }

    Connections {
        function onMatrixTimelineStateChanged() {
            if (!root.restoringEditDraft || root.activeEditEventId.length > 0)
                return;

            matrixComposerInputController.setText(root.draftBeforeEdit);
            root.draftBeforeEdit = "";
            root.restoringEditDraft = false;
            root.focusTextInput();
        }

        function onFocusInput() {
            root.focusTextInput();
        }

        target: TimelineManager
    }
}
