// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import cc.etke.komai

QtObject {
    id: support

    required property var rootItem

    function selectedEventIdsContains(eventId) {
        const normalizedEventId = String(eventId || "");
        return normalizedEventId.length > 0 && rootItem.selectedEventIds.indexOf(normalizedEventId) >= 0;
    }

    function canExplicitlySelectEventId(eventId) {
        const normalizedEventId = String(eventId || "");
        return normalizedEventId.length > 0
            && TimelineManager.matrixTimelineModel
            && TimelineManager.matrixTimelineModel.rowForEventId(normalizedEventId) >= 0;
    }

    function updateSelectionAnchor(preferredEventId) {
        const normalizedEventId = String(preferredEventId || "");
        if (support.selectedEventIdsContains(normalizedEventId)) {
            rootItem.selectionAnchorEventId = normalizedEventId;
            return;
        }

        rootItem.selectionAnchorEventId = rootItem.selectedEventIds.length > 0
            ? String(rootItem.selectedEventIds[rootItem.selectedEventIds.length - 1] || "")
            : "";
    }

    function toggleSelectionForEventId(eventId) {
        const normalizedEventId = String(eventId || "");
        if (!support.canExplicitlySelectEventId(normalizedEventId))
            return false;

        const wasSelected = support.selectedEventIdsContains(normalizedEventId);
        if (wasSelected) {
            rootItem.selectedEventIds = rootItem.selectedEventIds.filter(function (selectedEventId) {
                return String(selectedEventId || "") !== normalizedEventId;
            });
        } else {
            rootItem.selectedEventIds = rootItem.selectedEventIds.concat([normalizedEventId]);
        }

        support.updateSelectionAnchor(wasSelected ? "" : normalizedEventId);
        return true;
    }

    function registerVisibleDelegate(eventId, delegateItem) {
        const key = String(eventId || "");
        if (key.length === 0 || !delegateItem)
            return;

        rootItem.visibleTimelineDelegates[key] = delegateItem;
        rootItem.visibleTimelineDelegatesChanged();
    }

    function unregisterVisibleDelegate(eventId, delegateItem) {
        const key = String(eventId || "");
        if (key.length === 0)
            return;

        if (!rootItem.visibleTimelineDelegates[key])
            return;
        if (delegateItem && rootItem.visibleTimelineDelegates[key] !== delegateItem)
            return;

        delete rootItem.visibleTimelineDelegates[key];
        rootItem.visibleTimelineDelegatesChanged();
    }

    function replaceTrackedEventId(previousId, nextId) {
        const oldKey = String(previousId || "");
        const newKey = String(nextId || "");
        if (oldKey.length === 0 || newKey.length === 0 || oldKey === newKey)
            return;

        const tracked = rootItem.visibleTimelineDelegates[oldKey];
        if (!tracked)
            return;

        delete rootItem.visibleTimelineDelegates[oldKey];
        rootItem.visibleTimelineDelegates[newKey] = tracked;
        rootItem.visibleTimelineDelegatesChanged();

        if (rootItem.focusedEventId === oldKey)
            rootItem.focusedEventId = newKey;
        if (rootItem.selectionAnchorEventId === oldKey)
            rootItem.selectionAnchorEventId = newKey;
        if (rootItem.selectedEventIds.indexOf(oldKey) >= 0) {
            rootItem.selectedEventIds = rootItem.selectedEventIds.map(function (eventId) {
                return String(eventId || "") === oldKey ? newKey : eventId;
            });
        }
    }

    function matrixTimelineHeightCacheKey(eventId, itemId) {
        const stableEventId = String(eventId || "").trim();
        if (stableEventId.length > 0)
            return stableEventId;
        return String(itemId || "").trim();
    }

    function rememberedTimelineHeight(cacheKey) {
        if (!cacheKey || rootItem.measuredTimelineHeights[cacheKey] === undefined)
            return 0;
        return Number(rootItem.measuredTimelineHeights[cacheKey] || 0);
    }

    function rememberTimelineHeight(cacheKey, height) {
        const stableKey = String(cacheKey || "").trim();
        const stableHeight = Math.round(Number(height || 0));
        if (stableKey.length === 0 || stableHeight <= 0)
            return;
        if (Number(rootItem.measuredTimelineHeights[stableKey] || 0) === stableHeight)
            return;

        rootItem.measuredTimelineHeights[stableKey] = stableHeight;
        rootItem.measuredTimelineHeightsChanged();
    }

    function matrixEventTypeForItemKind(kind) {
        switch (kind) {
        case "notice":
            return MtxEvent.NoticeMessage;
        case "redacted":
            return MtxEvent.Redacted;
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
        return TimelineManager.formatMatrixMessageHtml(String(text || ""));
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

    function matrixRedactedEventPair(senderDisplayName, senderId) {
        const senderLabel = String(senderDisplayName || senderId || "").trim();
        if (senderLabel.length === 0) {
            return {
                "first": qsTr("Deleted message"),
                "second": ""
            };
        }

        return {
            "first": qsTr("Deleted message"),
            "second": qsTr("Originally sent by %1").arg(senderLabel)
        };
    }
}
