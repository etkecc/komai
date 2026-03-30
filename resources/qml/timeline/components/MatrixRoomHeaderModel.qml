// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Window
import cc.etke.komai

QtObject {
    id: root

    required property var rootItem
    required property var roomPreview
    required property var dialogRoomModel

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
    property AbstractPermissions permissions: PreviewPermissions {}
    property bool supportsSearch: false
    property bool supportsPinnedMessagesUi: true
    property bool supportsVisibilityInfo: true

    function previousVisibleItem(model, row) {
        if (!model)
            return ({});

        for (let candidateRow = row + 1; candidateRow < TimelineManager.matrixTimelineItemCount; candidateRow++) {
            const candidate = model.itemAt(candidateRow);
            if (candidate && !Boolean(candidate.isHiddenEvent))
                return candidate;
        }

        return ({});
    }

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

        const previousItem = root.previousVisibleItem(model, row);
        const timestamp = Number(item.timestamp || 0);
        const dayKey = root.rootItem.matrixTimelineDayKey(timestamp);
        const previousTimestamp = previousItem.timestamp !== undefined
            ? new Date(Number(previousItem.timestamp))
            : new Date(timestamp);
        const previousDay = previousItem.timestamp !== undefined
            ? root.rootItem.matrixTimelineDayKey(previousItem.timestamp)
            : dayKey;
        const previousIsStateEvent = previousItem.eventId === undefined
            ? true
            : root.rootItem.isMatrixStateLikeKind(previousItem.typeString);
        const previousUserId = previousItem.userId !== undefined
            ? String(previousItem.userId || "")
            : "";
        const itemKind = String(item.typeString || "");
        const itemType = item.type !== undefined
            ? Number(item.type)
            : root.rootItem.matrixEventTypeForItemKind(itemKind);
        const body = String(item.body || "");
        const effectiveFileName = item.filename && String(item.filename).length > 0
            ? String(item.filename)
            : (body.length > 0 ? body : qsTr("Attachment"));
        const humanReadableMediaSize = Number(item.filesizeBytes || 0) > 0
            ? Komai.humanReadableFileSize(Number(item.filesizeBytes))
            : "";
        const basePreview = {
            "room": root,
            "eventId": String(item.eventId || ""),
            "userId": String(item.userId || ""),
            "userName": String(item.userName || ""),
            "avatarUrl": String(item.senderAvatarUrl || ""),
            "previousDay": previousDay,
            "previousTimestamp": previousTimestamp,
            "previousIsStateEvent": previousIsStateEvent,
            "previousUserId": previousUserId
        };
        const redactedPair = root.rootItem.matrixRedactedEventPair(item.userName, item.userId);

        if (itemKind === "redacted") {
            return Object.assign({}, basePreview, {
                "type": itemType,
                "redactedFirst": redactedPair.first,
                "redactedSecond": redactedPair.second
            });
        }

        if (root.rootItem.isMatrixStateLikeKind(itemKind)) {
            return Object.assign({}, basePreview, {
                "type": itemType,
                "formattedStateEvent": root.rootItem.formattedMatrixTextHtml(body),
                "stateEventIconSource": root.rootItem.matrixStateEventIconForKind(itemKind)
            });
        }

        if (itemKind === "image" || itemKind === "sticker" || itemKind === "video") {
            const mediaWidth = Math.round(Number(item.originalWidth || 0));
            const mediaHeight = Math.round(Number(item.originalHeight || 0));
            const safePreviewAspectRatio = mediaWidth > 0 && mediaHeight > 0
                ? (mediaHeight / mediaWidth)
                : 0.75;
            return Object.assign({}, basePreview, {
                "type": itemType,
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
                "containerHeight": root.rootItem.height > 0 ? root.rootItem.height : Screen.height,
                "duration": Math.round(Number(item.duration || 0))
            });
        }

        if (itemKind === "file" || itemKind === "audio") {
            return Object.assign({}, basePreview, {
                "type": itemType,
                "body": body,
                "filename": effectiveFileName,
                "filesize": humanReadableMediaSize,
                "fileTypeIconSource": Komai.fileTypeIconSource(String(item.mimetype || "")),
                "mimetype": String(item.mimetype || ""),
                "duration": Math.round(Number(item.duration || 0))
            });
        }

        return Object.assign({}, basePreview, {
            "type": itemType,
            "body": body,
            "formattedBody": root.rootItem.formattedMatrixTextHtml(body),
            "formattedStateEvent": root.rootItem.formattedMatrixTextHtml(body),
            "stateEventIconSource": root.rootItem.matrixStateEventIconForKind(itemKind),
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
        return root.rootItem.jumpToLoadedMatrixEvent(String(eventId || ""));
    }

    function openUserProfile(userId) {
        root.dialogRoomModel.openUserProfile(userId);
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

        return root.rootItem.matrixRedactedEventPair("", "");
    }

    function unpin(eventId) {
        TimelineManager.unpinActiveMatrixTimelineEvent(String(eventId || ""));
    }
}
