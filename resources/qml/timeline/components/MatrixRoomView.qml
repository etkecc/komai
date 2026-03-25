// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../room/components"
import "../../components" as Components
import "../../composer" as Composer
import "../styles/bubble"
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

        chatRoot: root.chatRoot ? root.chatRoot : matrixTimelineList
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
        chatRoot: root.chatRoot ? root.chatRoot : matrixTimelineList
        emojiPopup: root.emojiPopup
        filteredTimeline: root.filteredTimeline
        roomModel: matrixMessageActionsDefaultRoomModel
    }

    anchors.fill: parent
    enabled: visible
    spacing: 0
    visible: !!roomPreview && roomPreview.isMatrixSummary

    RoomHeader {
        Layout.fillWidth: true
        room: null
        roomModel: null
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

                        function formatBytes(bytes) {
                            const value = Number(bytes);
                            if (!isFinite(value) || value <= 0)
                                return "";

                            const units = ["B", "KB", "MB", "GB"];
                            let size = value;
                            let unitIndex = 0;
                            while (size >= 1024 && unitIndex < units.length - 1) {
                                size /= 1024;
                                unitIndex += 1;
                            }

                            return (size >= 10 || unitIndex === 0 ? Math.round(size) : size.toFixed(1)) + " " + units[unitIndex];
                        }

                        function formatDuration(durationMs) {
                            const value = Number(durationMs);
                            if (!isFinite(value) || value <= 0)
                                return "";

                            const totalSeconds = Math.round(value / 1000);
                            const minutes = Math.floor(totalSeconds / 60);
                            const seconds = totalSeconds % 60;
                            return minutes + ":" + (seconds < 10 ? "0" + seconds : seconds);
                        }

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
                        readonly property bool showCaption: isMediaItem && body.length > 0 && body !== effectiveFileName
                        readonly property bool hasReplyPreview: replyBody.length > 0
                        readonly property bool showVisualPreview: ["image", "video", "sticker"].indexOf(itemKind) >= 0 && itemId.length > 0
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
                        readonly property bool messageIsRightAligned: isOwn
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
                        readonly property string mediaKindLabel: {
                            switch (itemKind) {
                            case "image":
                                return qsTr("Image");
                            case "video":
                                return qsTr("Video");
                            case "audio":
                                return qsTr("Audio");
                            case "sticker":
                                return qsTr("Sticker");
                            case "file":
                            default:
                                return qsTr("File");
                            }
                        }
                        readonly property string mediaMetaText: {
                            const parts = [];
                            if (mimeType.length > 0)
                                parts.push(mimeType);
                            const dimensions = mediaWidth > 0 && mediaHeight > 0 ? (Math.round(mediaWidth) + "×" + Math.round(mediaHeight)) : "";
                            if (dimensions.length > 0)
                                parts.push(dimensions);
                            const durationText = formatDuration(mediaDurationMs);
                            if (durationText.length > 0)
                                parts.push(durationText);
                            const sizeText = formatBytes(mediaSizeBytes);
                            if (sizeText.length > 0)
                                parts.push(sizeText);
                            return parts.join(" · ");
                        }

                        width: matrixTimelineList.width
                        height: itemKind === "date_divider"
                            ? dateDivider.implicitHeight
                            : usesSharedTimelineBubble
                                ? sharedTextBubble.height
                                : messageRow.implicitHeight

                        function openMessageActions(pin, anchorItem, activationMode) {
                            const actionsControl = matrixMessageActionsHost.control;
                            if (!actionsControl || !anchorItem)
                                return;

                            if (hoverDismissTimer.running)
                                hoverDismissTimer.stop();

                            const resolvedActivationMode = activationMode !== undefined
                                ? activationMode
                                : (pin ? "button" : "hover");

                            actionsControl.model = matrixToolbarMessageModel;
                            actionsControl.roomModelOverride = matrixToolbarRoomModel;
                            actionsControl.attached = timelineItemDelegate;
                            actionsControl.activationMode = resolvedActivationMode;
                            actionsControl.anchorItem = anchorItem;
                            actionsControl.positioned = false;

                            Qt.callLater(function () {
                                timelineItemDelegate.repositionMessageActions(anchorItem, resolvedActivationMode, 0);
                            });
                        }

                        function repositionMessageActions(anchorItem, activationMode, attempt) {
                            const actionsControl = matrixMessageActionsHost.control;
                            if (!actionsControl || !anchorItem)
                                return;

                            if (attempt === undefined)
                                attempt = 0;

                            if (activationMode === undefined || activationMode === null || activationMode === "")
                                activationMode = actionsControl.activationMode;

                            if (attempt > 60)
                                return;

                            const nextAttempt = attempt + 1;
                            const actionsParent = actionsControl.parent ? actionsControl.parent : matrixTimelineList.contentItem;
                            if (!actionsParent) {
                                Qt.callLater(function () {
                                    timelineItemDelegate.repositionMessageActions(anchorItem, activationMode, nextAttempt);
                                });
                                return;
                            }

                            const pos = anchorItem.mapToItem(actionsParent, 0, 0);
                            const wrapperPos = timelineItemDelegate.mapToItem(actionsParent, 0, 0);
                            const barW = actionsControl.implicitWidth;
                            const barH = actionsControl.implicitHeight;
                            const chatWidth = matrixTimelineList.width;
                            const chatHeight = matrixTimelineList.height;

                            if (barW <= 0 || barH <= 0 || chatWidth <= 0 || chatHeight <= 0) {
                                Qt.callLater(function () {
                                    timelineItemDelegate.repositionMessageActions(anchorItem, activationMode, nextAttempt);
                                });
                                return;
                            }

                            const viewportTop = actionsParent === matrixTimelineList.contentItem ? matrixTimelineList.contentY : 0;
                            const viewportBottom = viewportTop + chatHeight;
                            const targetY = pos.y - barH;
                            actionsControl.y = Math.max(viewportTop, Math.min(targetY, viewportBottom - barH));

                            const viewportLeft = 0;
                            const viewportRight = chatWidth;
                            let minX = wrapperPos.x + Komai.paddingLarge;
                            let maxX = wrapperPos.x + timelineItemDelegate.width - Komai.paddingLarge - barW;
                            if (maxX < minX) {
                                minX = viewportLeft;
                                maxX = viewportRight - barW;
                            }

                            if (activationMode === "button") {
                                const centerX = pos.x + anchorItem.width / 2 - barW / 2;
                                actionsControl.x = Math.max(minX, Math.min(centerX, maxX));
                            } else {
                                actionsControl.x = messageIsRightAligned ? maxX : minX;
                            }

                            actionsControl.x = Math.max(viewportLeft, Math.min(actionsControl.x, viewportRight - barW));
                            actionsControl.positioned = true;
                        }

                        function isHoverActionsEnabled() {
                            return Settings.timelineMessageActionsActivationPolicy === Settings.TimelineMessageActionsActivationPolicy.OnHover;
                        }

                        function isUnpinnedActionBarAttached() {
                            const actionsControl = matrixMessageActionsHost.control;
                            return !!actionsControl
                                && actionsControl.attached === timelineItemDelegate
                                && actionsControl.activationMode === "hover";
                        }

                        function handleMessageHoverChanged(isHovered, anchorItem) {
                            const actionsControl = matrixMessageActionsHost.control;
                            if (!actionsControl || !isHoverActionsEnabled() || actionsControl.activationMode === "keyboard")
                                return;

                            if (isHovered) {
                                if (hoverDismissTimer.running)
                                    hoverDismissTimer.stop();
                                openMessageActions(false, anchorItem, "hover");
                            } else if (isUnpinnedActionBarAttached()) {
                                hoverDismissTimer.restart();
                            }
                        }

                        function handleHoverDismissTimerTriggered(isHovered) {
                            const actionsControl = matrixMessageActionsHost.control;
                            if (!actionsControl || !isHoverActionsEnabled() || actionsControl.activationMode === "keyboard")
                                return;
                            if (!isUnpinnedActionBarAttached())
                                return;
                            if (isHovered || actionsControl.hovered)
                                return;
                            actionsControl.dismiss();
                        }

                        function togglePinnedMessageActions(anchorItem) {
                            const actionsControl = matrixMessageActionsHost.control;
                            if (!actionsControl)
                                return;

                            if (actionsControl.pinned && actionsControl.attached === timelineItemDelegate)
                                actionsControl.dismiss();
                            else
                                openMessageActions(true, anchorItem, "button");
                        }

                        QtObject {
                            id: matrixToolbarNoopControl

                            function dismiss() {
                            }
                        }

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
                            property string reply: ""
                            property string edit: ""
                            property string thread: ""

                            function formatDateSeparator(timestamp) {
                                return Qt.formatDate(timestamp, "ddd, MMM d");
                            }

                            function formatLaterSeparator(_previous, currentTimestamp) {
                                return Qt.formatTime(currentTimestamp, "hh:mm");
                            }

                            function openUserProfile(_userId) {
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
                            readonly property bool supportsRemove: false
                            readonly property bool supportsViewRaw: false
                            readonly property bool supportsReadReceipts: false
                            readonly property bool supportsMarkAsRead: false
                            readonly property bool supportsPin: false
                            readonly property bool supportsReport: false
                            readonly property bool supportsOpenMedia: timelineItemDelegate.isMediaItem
                            readonly property bool supportsSaveMedia: timelineItemDelegate.isMediaItem
                            readonly property bool supportsCopyEventLink: false
                        }

                        TapHandler {
                            acceptedButtons: Qt.RightButton
                            gesturePolicy: TapHandler.ReleaseWithinBounds
                            enabled: !usesSharedTimelineBubble

                            onSingleTapped: root.openMatrixMessageContextMenu(
                                matrixToolbarMessageModel,
                                matrixToolbarRoomModel,
                                timelineItemDelegate.body.length > 0
                                    ? timelineItemDelegate.body
                                    : timelineItemDelegate.effectiveFileName)
                        }

                        Timer {
                            id: hoverDismissTimer

                            interval: 180
                            repeat: false
                            onTriggered: timelineItemDelegate.handleHoverDismissTimerTriggered(messageBubbleHover.hovered)
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

                        Item {
                            id: messageRow

                            anchors.left: parent.left
                            anchors.right: parent.right
                            implicitHeight: messageRowLayout.implicitHeight
                            visible: itemKind !== "date_divider" && !usesSharedTimelineBubble

                            RowLayout {
                                id: messageRowLayout

                                anchors.left: isOwn ? undefined : parent.left
                                anchors.right: isOwn ? parent.right : undefined
                                spacing: Komai.paddingSmall
                                width: Math.min(parent.width * 0.9, Math.max(320, parent.width * 0.7))

                                Avatar {
                                    Layout.alignment: Qt.AlignTop
                                    crop: true
                                    displayName: senderDisplayName
                                    implicitHeight: Komai.listIconSize
                                    implicitWidth: Komai.listIconSize
                                    roomid: root.roomPreview ? root.roomPreview.roomid : ""
                                    url: senderAvatarUrl.replace("mxc://", "image://MxcImage/")
                                    userid: senderId
                                    visible: !isOwn
                                }

                                Item {
                                    Layout.preferredWidth: visible ? Komai.paddingSmall : 0
                                    visible: !isOwn
                                }

                                ColumnLayout {
                                    Layout.alignment: isOwn ? Qt.AlignRight : Qt.AlignLeft
                                    Layout.preferredWidth: Math.min(messageRow.width * 0.82, Math.max(280, messageRow.width * 0.6))
                                    spacing: Komai.paddingSmall

                                    MatrixText {
                                        Layout.alignment: isOwn ? Qt.AlignRight : Qt.AlignLeft
                                        color: palette.buttonText
                                        text: senderDisplayName
                                        textFormat: TextEdit.PlainText
                                    }

                                    Rectangle {
                                        id: messageBubble

                                        Layout.alignment: isOwn ? Qt.AlignRight : Qt.AlignLeft
                                        color: isOwn ? palette.highlight : palette.alternateBase
                                        implicitHeight: bubbleContent.implicitHeight + Komai.paddingMedium * 2
                                        implicitWidth: Math.min(parent.width, bubbleContent.implicitWidth + Komai.paddingLarge * 2)
                                        radius: Komai.paddingMedium * 2

                                        HoverHandler {
                                            id: messageBubbleHover

                                            blocking: false
                                            onHoveredChanged: timelineItemDelegate.handleMessageHoverChanged(
                                                hovered,
                                                footerMetadata)
                                        }

                                        ColumnLayout {
                                            id: bubbleContent
                                            anchors.fill: parent
                                            anchors.margins: Komai.paddingMedium
                                            spacing: Komai.paddingSmall
                                            width: parent.width - Komai.paddingMedium * 2

                                            Rectangle {
                                                Layout.fillWidth: true
                                                color: Qt.rgba(palette.base.r, palette.base.g, palette.base.b, isOwn ? 0.22 : 0.5)
                                                implicitHeight: replyPreviewLayout.implicitHeight + Komai.paddingSmall * 2
                                                radius: Komai.paddingMedium
                                                visible: hasReplyPreview

                                                TapHandler {
                                                    acceptedButtons: Qt.LeftButton | Qt.RightButton
                                                    gesturePolicy: TapHandler.ReleaseWithinBounds

                                                    onSingleTapped: eventPoint => {
                                                        if (eventPoint.button === Qt.RightButton) {
                                                            matrixReplyContextMenu.show(replyBody, "", replyEventId);
                                                            return;
                                                        }

                                                        root.jumpToLoadedMatrixEvent(replyEventId);
                                                    }
                                                }

                                                Components.KomaiCursorShape {
                                                    anchors.fill: parent
                                                    cursorShape: replyEventId.length > 0
                                                        ? Qt.PointingHandCursor
                                                        : Qt.ArrowCursor
                                                }

                                                ColumnLayout {
                                                    id: replyPreviewLayout

                                                    anchors.fill: parent
                                                    anchors.margins: Komai.paddingSmall
                                                    spacing: Math.max(2, Math.round(Komai.paddingSmall / 2))

                                                    MatrixText {
                                                        Layout.fillWidth: true
                                                        color: isOwn ? palette.highlightedText : palette.text
                                                        font.bold: true
                                                        text: replySenderDisplayName.length > 0 ? replySenderDisplayName : qsTr("Reply")
                                                        textFormat: TextEdit.PlainText
                                                        wrapMode: Text.WordWrap
                                                    }

                                                    MatrixText {
                                                        Layout.fillWidth: true
                                                        color: isOwn ? palette.highlightedText : palette.buttonText
                                                        text: replyBody
                                                        textFormat: TextEdit.PlainText
                                                        wrapMode: Text.WordWrap
                                                    }
                                                }
                                            }

                                            MatrixText {
                                                id: bubbleBody

                                                Layout.fillWidth: true
                                                color: isOwn ? palette.highlightedText : palette.text
                                                text: body
                                                textFormat: TextEdit.PlainText
                                                visible: !isMediaItem
                                                wrapMode: Text.WordWrap
                                            }

                                            Rectangle {
                                                Layout.fillWidth: true
                                                color: Qt.rgba(palette.base.r, palette.base.g, palette.base.b, isOwn ? 0.2 : 0.55)
                                                implicitHeight: mediaCardLayout.implicitHeight + Komai.paddingMedium * 2
                                                radius: Komai.paddingMedium * 1.5
                                                visible: isMediaItem

                                                ColumnLayout {
                                                    id: mediaCardLayout

                                                    anchors.fill: parent
                                                    anchors.margins: Komai.paddingMedium
                                                    spacing: Komai.paddingSmall

                                                    MatrixText {
                                                        Layout.fillWidth: true
                                                        color: isOwn ? palette.highlightedText : palette.text
                                                        text: mediaKindLabel
                                                        textFormat: TextEdit.PlainText
                                                    }

                                                    Rectangle {
                                                        Layout.fillWidth: true
                                                        color: palette.base
                                                        implicitHeight: Math.round(Math.min(280, Math.max(120, width * safePreviewAspectRatio)))
                                                        radius: Komai.paddingMedium
                                                        visible: showVisualPreview

                                                        Image {
                                                            anchors.fill: parent
                                                            anchors.margins: 1
                                                            asynchronous: true
                                                            fillMode: Image.PreserveAspectFit
                                                            source: parent.visible
                                                                ? ("image://MxcImage/matrix-timeline:" + itemId + "?scale")
                                                                : ""
                                                            sourceSize.width: Math.max(320, width * Screen.devicePixelRatio)
                                                            sourceSize.height: Math.max(180, parent.height * Screen.devicePixelRatio)
                                                            smooth: true
                                                        }

                                                        MouseArea {
                                                            anchors.fill: parent
                                                            cursorShape: Qt.PointingHandCursor

                                                            onClicked: TimelineManager.openActiveMatrixTimelineMedia(itemId, effectiveFileName)
                                                        }
                                                    }

                                                    MatrixText {
                                                        Layout.fillWidth: true
                                                        color: isOwn ? palette.highlightedText : palette.text
                                                        font.bold: true
                                                        text: effectiveFileName
                                                        textFormat: TextEdit.PlainText
                                                        wrapMode: Text.WordWrap
                                                    }

                                                    MatrixText {
                                                        Layout.fillWidth: true
                                                        color: isOwn ? palette.highlightedText : palette.text
                                                        text: body
                                                        textFormat: TextEdit.PlainText
                                                        visible: showCaption
                                                        wrapMode: Text.WordWrap
                                                    }

                                                    MatrixText {
                                                        Layout.fillWidth: true
                                                        color: isOwn ? palette.highlightedText : palette.buttonText
                                                        text: mediaMetaText
                                                        textFormat: TextEdit.PlainText
                                                        visible: mediaMetaText.length > 0
                                                        wrapMode: Text.WordWrap
                                                    }

                                                    MatrixText {
                                                        Layout.fillWidth: true
                                                        color: isOwn ? palette.highlightedText : palette.buttonText
                                                        text: mediaIsEncrypted
                                                            ? qsTr("Encrypted attachment. Open and save are handled through the Rust matrix-sdk backend.")
                                                            : qsTr("Open and save are handled through the Rust matrix-sdk backend.")
                                                        textFormat: TextEdit.PlainText
                                                        wrapMode: Text.WordWrap
                                                    }

                                                    RowLayout {
                                                        Layout.fillWidth: true
                                                        spacing: Komai.paddingSmall

                                                        Components.KomaiButton {
                                                            text: qsTr("Open")

                                                            onClicked: TimelineManager.openActiveMatrixTimelineMedia(itemId, effectiveFileName)
                                                        }

                                                        Components.KomaiButton {
                                                            text: qsTr("Save")

                                                            onClicked: TimelineManager.saveActiveMatrixTimelineMedia(itemId, effectiveFileName)
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }

                                    Reactions {
                                        Layout.alignment: isOwn ? Qt.AlignRight : Qt.AlignLeft
                                        eventId: timelineItemDelegate.eventId
                                        layoutDirection: isOwn ? Qt.RightToLeft : Qt.LeftToRight
                                        reactions: timelineItemDelegate.reactions
                                        roomModel: matrixToolbarRoomModel
                                        visible: reactions.length > 0
                                        width: messageBubble.width
                                    }

                                    TimelineMetadata {
                                        id: footerMetadata
                                        Layout.alignment: isOwn ? Qt.AlignRight : Qt.AlignLeft
                                        forceTrailingTimestampLayout: true
                                        scaling: 0.9
                                        eventId: timelineItemDelegate.eventId
                                        status: MtxEvent.Empty
                                        trustlevel: 0
                                        isEdited: timelineItemDelegate.isEdited
                                        isEncrypted: timelineItemDelegate.mediaIsEncrypted
                                            || timelineItemDelegate.thumbnailIsEncrypted
                                            || timelineItemDelegate.itemKind === "unable_to_decrypt"
                                        isStateEvent: timelineItemDelegate.isStateLikeItem
                                        threadId: ""
                                        timestamp: new Date(timelineItemDelegate.timestamp)
                                        room: matrixToolbarRoomModel
                                        isSender: timelineItemDelegate.isOwn
                                        actionBarActive: matrixMessageActionsHost.control.pinned
                                            && matrixMessageActionsHost.control.attached === timelineItemDelegate

                                        onActionToggled: timelineItemDelegate.togglePinnedMessageActions(
                                            actionToggleButton)
                                    }
                                }
                            }
                        }

                        TimelineBubbleMessageStyle {
                            id: sharedTextBubble

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
                            visible: timelineItemDelegate.usesSharedTimelineBubble
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
