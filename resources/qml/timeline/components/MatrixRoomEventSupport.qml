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
        const model = rootItem.activeTimelineModel;
        if (normalizedEventId.length === 0 || !model)
            return false;

        const row = model.rowForEventId(normalizedEventId);
        return row >= 0 && rootItem.isSelectableMatrixTimelineRow(row);
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

    function selectRangeToEventId(eventId) {
        const normalizedEventId = String(eventId || "");
        if (!support.canExplicitlySelectEventId(normalizedEventId))
            return false;

        const anchorEventId = String(rootItem.selectionAnchorEventId || "");
        if (anchorEventId.length === 0 || anchorEventId === normalizedEventId)
            return support.toggleSelectionForEventId(normalizedEventId);

        const model = rootItem.activeTimelineModel;
        if (!model)
            return false;

        const anchorRow = model.rowForEventId(anchorEventId);
        const endRow = model.rowForEventId(normalizedEventId);
        if (anchorRow < 0 || endRow < 0)
            return support.toggleSelectionForEventId(normalizedEventId);

        const minRow = Math.min(anchorRow, endRow);
        const maxRow = Math.max(anchorRow, endRow);

        const additions = [];
        const seen = {};
        const existing = rootItem.selectedEventIds;
        for (let i = 0; i < existing.length; i += 1)
            seen[String(existing[i] || "")] = true;

        for (let row = minRow; row <= maxRow; row += 1) {
            if (!rootItem.isSelectableMatrixTimelineRow(row))
                continue;
            const item = model.itemAt(row);
            if (!item)
                continue;
            const rowEventId = String(item.eventId || "");
            if (rowEventId.length === 0 || seen[rowEventId])
                continue;
            seen[rowEventId] = true;
            additions.push(rowEventId);
        }

        if (additions.length > 0)
            rootItem.selectedEventIds = existing.concat(additions);

        return true;
    }

    function registerVisibleDelegate(eventId, delegateItem) {
        const key = String(eventId || "");
        if (key.length === 0 || !delegateItem)
            return;

        rootItem.visibleTimelineDelegates[key] = delegateItem;
        rootItem.delegateRegistrationRevision += 1;
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
        rootItem.delegateRegistrationRevision += 1;
        rootItem.visibleTimelineDelegatesChanged();
    }

    function replaceTrackedEventId(previousId, nextId) {
        const oldKey = String(previousId || "");
        const newKey = String(nextId || "");
        if (oldKey.length === 0 || newKey.length === 0 || oldKey === newKey)
            return;

        // A delegate's eventId changes for two very different reasons:
        //   1. A local echo gains its real server event ID (same message, same
        //      delegate, updated in place). The old (temporary) ID stops existing
        //      as its own timeline row, so selection/focus must follow it across.
        //   2. The ListView recycles the delegate (reuseItems) onto an unrelated
        //      message while scrolling. Here BOTH IDs are live, distinct events.
        // Only case 1 should re-point selection/focus/anchor. If the old ID still
        // resolves to a live timeline row, this is case 2 (a recycle) and remapping
        // would hijack the selection onto the recycled message, so bail out.
        const model = rootItem.activeTimelineModel;
        if (model) {
            const oldRow = typeof model.rawRowForEventId === "function"
                ? model.rawRowForEventId(oldKey)
                : model.rowForEventId(oldKey);
            if (oldRow >= 0)
                return;
        }

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
        case "message":
            return MtxEvent.TextMessage;
        case "notice":
            return MtxEvent.NoticeMessage;
        case "emote":
            return MtxEvent.EmoteMessage;
        case "redacted":
            return MtxEvent.Redacted;
        case "unable_to_decrypt":
            return MtxEvent.Encrypted;
        case "image":
            return MtxEvent.ImageMessage;
        case "video":
            return MtxEvent.VideoMessage;
        case "audio":
            return MtxEvent.AudioMessage;
        case "file":
            return MtxEvent.FileMessage;
        case "location":
            return MtxEvent.LocationMessage;
        case "sticker":
            return MtxEvent.Sticker;
        case "membership_change":
        case "profile_change":
            return MtxEvent.Member;
        case "unknown_message":
        case "failed_to_parse_message_like":
            return MtxEvent.UnknownMessage;
        default:
            return MtxEvent.UnknownEvent;
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
            return ":/icons/icons/ui/state-member-change.svg";
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
