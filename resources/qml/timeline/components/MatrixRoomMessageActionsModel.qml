// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import cc.etke.komai

Item {
    id: root

    required property var rootItem
    required property var roomPreview
    required property var dialogRoomModel
    required property var headerRoomModel
    required property var openForwardDialogFn

    width: 0
    height: 0

    PreviewPermissions {
        id: messageActionsPermissions
    }

    readonly property var toolbarInput: matrixTimelineToolbarInput

    property string roomId: roomPreview ? roomPreview.roomid : ""
    property bool isActiveMatrixTimelineRoom: true
    property int roomMemberCount: roomPreview && roomPreview.roomMemberCount !== undefined
        ? Number(roomPreview.roomMemberCount) : 0
    property bool isEncrypted: roomPreview ? !!roomPreview.isEncrypted : false
    property var permissions: messageActionsPermissions
    property var input: matrixTimelineToolbarInput
    property var frequentReactions: []
    property var pinnedMessages: TimelineManager.matrixTimelinePinnedEventIds
    property string reply: ""
    property string edit: ""
    property string thread: ""
    property bool supportsThreadNavigation: false

    function showEvent(eventId) {
        return root.rootItem.jumpToLoadedMatrixEvent(eventId);
    }

    function openForwardDialog(eventId) {
        return root.openForwardDialogFn(eventId);
    }

    function formatRedactedEvent(eventId) {
        const model = TimelineManager.matrixTimelineModel;
        if (!model)
            return root.rootItem.matrixRedactedEventPair("", "");

        const eid = String(eventId || "");
        return root.rootItem.matrixRedactedEventPair(model.userNameForEvent(eid),
                                                     model.userIdForEvent(eid));
    }

    function previewDataForEvent(eventId) {
        const preview = root.headerRoomModel.previewDataForEvent(eventId);
        return Object.assign({}, preview || {}, {
            "room": root
        });
    }

    function formatDateSeparator(timestamp) {
        return Qt.formatDate(timestamp, "ddd, MMM d");
    }

    function formatLaterSeparator(_previous, currentTimestamp) {
        return Qt.formatTime(currentTimestamp, "hh:mm");
    }

    function openUserProfile(userId) {
        root.dialogRoomModel.openUserProfile(userId);
    }

    function eventShown() {}
    function showImage() {
        return true;
    }

    function openMedia(targetEventId) {
        const model = TimelineManager.matrixTimelineModel;
        const eid = String(targetEventId || "");
        const fileName = model ? (model.filenameForEvent(eid) || qsTr("Attachment")) : qsTr("Attachment");
        TimelineManager.openActiveMatrixTimelineMedia(eid, fileName);
    }

    function openMediaOverlay(targetEventId) {
        const model = TimelineManager.matrixTimelineModel;
        const eid = String(targetEventId || "");
        if (!model || eid.length === 0)
            return false;

        const row = model.rowForEventId(eid);
        if (row < 0)
            return false;

        const item = model.itemAt(row);
        const url = String((item && item.url) || "");
        if (url.length === 0)
            return false;

        TimelineManager.openMediaOverlay(null,
                                         url,
                                         eid,
                                         Number((item && item.originalWidth) || 0),
                                         Number((item && item.proportionalHeight) || 0),
                                         Number((item && item.type) || -1),
                                         Number((item && item.duration) || 0),
                                         String((item && item.thumbnailUrl) || ""));
        return true;
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
        TimelineManager.reportActiveMatrixTimelineEvent(String(eventId || ""),
                                                       String(reason || ""),
                                                       Number(score || -50));
    }

    function viewRawMessage(eventId) {
        root.rootItem.openRawMessageDialog(String(eventId || ""));
    }

    function viewDecryptedRawMessage(eventId) {
        viewRawMessage(eventId);
    }

    function showReadReceipts(eventId) {
        root.rootItem.openReadReceiptsDialog(String(eventId || ""));
    }

    onReplyChanged: {
        if (!reply)
            return;

        const model = TimelineManager.matrixTimelineModel;
        TimelineManager.queueActiveMatrixReply(reply,
                                               model ? model.userIdForEvent(reply) : "",
                                               model ? model.userNameForEvent(reply) : "",
                                               model ? model.bodyForEvent(reply) : "");
        reply = "";
    }

    onEditChanged: {
        if (!edit)
            return;

        const model = TimelineManager.matrixTimelineModel;
        root.rootItem.beginEdit(edit,
                                model ? model.bodyForEvent(edit) : "",
                                model ? model.typeStringForEvent(edit) : "");
        edit = "";
    }

    onThreadChanged: {
        if (thread)
            thread = "";
    }

    QtObject {
        id: matrixTimelineToolbarInput

        function reaction(targetEventId, reactionKey) {
            TimelineManager.toggleActiveMatrixTimelineReaction(String(targetEventId || ""),
                                                              String(reactionKey || ""));
        }
    }
}
