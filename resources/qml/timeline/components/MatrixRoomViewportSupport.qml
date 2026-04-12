// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import cc.etke.komai

QtObject {
    id: support

    required property var rootItem
    required property var timelineList

    property bool destroyed: false
    Component.onDestruction: destroyed = true

    property var readMarkerUpdateTimer: Timer {
        interval: 0
        onTriggered: support.updateReadMarkerForVisibleContent()
    }
    property int scheduledReadMarkerGeneration: -1
    property string scheduledReadMarkerRoomId: ""

    function resetReadMarkerState() {
        readMarkerUpdateTimer.stop();
        scheduledReadMarkerGeneration = -1;
        scheduledReadMarkerRoomId = "";
    }

    function scheduleInitialTimelineBufferCheck() {
        if (rootItem.initialBufferCheckQueued)
            return;

        rootItem.initialBufferCheckQueued = true;
        Qt.callLater(function() {
            if (support.destroyed)
                return;
            rootItem.initialBufferCheckQueued = false;
            support.maybeRequestInitialTimelineBuffer();
        });
    }

    function scheduleDeferredInitialTimelineBufferCheck() {
        if (rootItem.deferredBufferCheckQueued)
            return;

        rootItem.deferredBufferCheckQueued = true;
        Qt.callLater(function() {
            if (support.destroyed)
                return;
            rootItem.deferredBufferCheckQueued = false;
            support.maybeRequestDeferredInitialTimelineBuffer();
        });
    }

    function bottomMostVisibleDelegate() {
        const viewportTop = timelineList ? timelineList.contentY : 0;
        const viewportBottom = viewportTop + (timelineList ? timelineList.height : 0);
        let candidate = null;
        let candidateBottom = -1;

        for (const eventId in rootItem.visibleTimelineDelegates) {
            const delegateItem = rootItem.visibleTimelineDelegates[eventId];
            if (!delegateItem || !delegateItem.visible || delegateItem.height <= 0)
                continue;

            const top = Number(delegateItem.y || 0);
            const bottom = top + Number(delegateItem.height || 0);
            if (bottom <= viewportTop || top >= viewportBottom)
                continue;

            if (bottom > candidateBottom) {
                candidate = delegateItem;
                candidateBottom = bottom;
            }
        }

        return candidate;
    }

    function latestLoadedSelectableEventId() {
        for (let row = 0; row < (rootItem.perRoomModel ? rootItem.perRoomModel.count : 0); row++) {
            if (!rootItem.isSelectableMatrixTimelineRow(row))
                continue;

            const item = rootItem.perRoomModel.itemAt(row);
            const eventId = String(item.eventId || "");
            if (eventId.length > 0)
                return eventId;
        }

        return "";
    }

    function isEffectivelyAtLiveEdge() {
        if (!timelineList)
            return false;

        if (timelineList.keepPinnedToBottom || rootItem.initialBottomPinPending || timelineList.atYEnd)
            return true;

        if (timelineList.userUnpinned)
            return false;

        const viewportHeight = Number(timelineList.height || 0);
        const contentHeight = Number(timelineList.contentHeight || 0);
        return viewportHeight > 0 && contentHeight > 0 && contentHeight <= viewportHeight + 2;
    }

    function selectableEventIdNearMatrixRow(row) {
        const model = rootItem.perRoomModel;
        const rowCount = (rootItem.perRoomModel ? rootItem.perRoomModel.count : 0);
        if (!model || row < 0 || row >= rowCount)
            return "";

        const offsets = [0, -1, 1, -2, 2];
        for (let i = 0; i < offsets.length; i++) {
            const candidateRow = row + offsets[i];
            if (candidateRow < 0 || candidateRow >= rowCount)
                continue;

            const item = model.itemAt(candidateRow);
            if (!item || item === undefined)
                continue;

            const eventId = String(item.eventId || "");
            const itemKind = String(item.typeString || "");
            if (eventId.length === 0 || itemKind === "date_divider" || Boolean(item.isHiddenEvent))
                continue;

            return eventId;
        }

        return "";
    }

    function bottomMostVisibleEventId() {
        if (support.isEffectivelyAtLiveEdge()) {
            const latestEventId = support.latestLoadedSelectableEventId();
            if (latestEventId.length > 0)
                return latestEventId;
        }

        if (!timelineList || !rootItem.perRoomModel || timelineList.width <= 0
                || timelineList.height <= 0) {
            const delegateItem = support.bottomMostVisibleDelegate();
            return delegateItem && delegateItem.eventId
                ? String(delegateItem.eventId || "")
                : "";
        }

        const probeX = Math.max(1, Math.round(timelineList.width / 2));
        const probeY = Math.max(1, Math.round(timelineList.height - 2));
        const row = timelineList.indexAt(probeX, probeY);
        if (row >= 0) {
            const eventId = support.selectableEventIdNearMatrixRow(row);
            if (eventId.length > 0)
                return eventId;
        }

        const delegateItem = support.bottomMostVisibleDelegate();
        if (delegateItem && delegateItem.eventId)
            return String(delegateItem.eventId || "");

        return "";
    }

    function scheduleReadMarkerUpdate(preferLatestEvent) {
        if (!rootItem.visible
                || rootItem.activeRoomId.length === 0
                || !rootItem.hasTimeline
                || rootItem.loading
                || rootItem.roomSwitchInProgress
                || rootItem.initialBottomPinPending) {
            return;
        }

        rootItem.preferLatestReadMarkerEvent = rootItem.preferLatestReadMarkerEvent || !!preferLatestEvent;
        scheduledReadMarkerGeneration = Number(rootItem.readMarkerGeneration || 0);
        scheduledReadMarkerRoomId = rootItem.activeRoomId;
        readMarkerUpdateTimer.restart();
    }

    function updateReadMarkerForVisibleContent() {
        if (!rootItem.visible
                || rootItem.activeRoomId.length === 0
                || !rootItem.hasTimeline
                || rootItem.loading
                || rootItem.roomSwitchInProgress
                || rootItem.initialBottomPinPending
                || scheduledReadMarkerRoomId !== rootItem.activeRoomId
                || scheduledReadMarkerGeneration !== Number(rootItem.readMarkerGeneration || 0)) {
            rootItem.preferLatestReadMarkerEvent = false;
            return;
        }

        let targetEventId = "";
        if (rootItem.preferLatestReadMarkerEvent || support.isEffectivelyAtLiveEdge())
            targetEventId = support.latestLoadedSelectableEventId();
        if (targetEventId.length === 0)
            targetEventId = support.bottomMostVisibleEventId();

        rootItem.preferLatestReadMarkerEvent = false;

        if (targetEventId.length === 0 || targetEventId === rootItem.lastMarkedReadEventId)
            return;

        TimelineManager.markActiveMatrixTimelineEventAsRead(targetEventId);
        rootItem.lastMarkedReadEventId = targetEventId;
    }

    function ensureInitialBottomPin() {
        const roomId = rootItem.activeRoomId;
        if (!timelineList
                || roomId.length === 0
                || rootItem.loading
                || !rootItem.hasTimeline) {
            return;
        }

        if (timelineList.visibleIndicesValid)
            return;

        rootItem.initialBottomPinPending = true;
        timelineList.keepPinnedToBottom = true;
        timelineList.maybeScrollToBottom(true);

        Qt.callLater(function () {
            if (support.destroyed
                    || !timelineList
                    || rootItem.activeRoomId !== roomId
                    || rootItem.loading
                    || !rootItem.hasTimeline) {
                return;
            }

            timelineList.forceLayout();
            if (timelineList.keepPinnedToBottom)
                timelineList.maybeScrollToBottom(true);
            timelineList.updateLastScroll();
            if (timelineList.atYEnd)
                rootItem.initialBottomPinPending = false;
        });
    }

    function maybeRequestInitialTimelineBuffer() {
        if (!timelineList
                || !rootItem.initialTimelineBufferPending
                || rootItem.initialBottomPinPending
                || rootItem.bufferPaginationInFlight
                || rootItem.loading
                || !rootItem.hasTimeline) {
            return;
        }

        const viewportHeight = timelineList.height;
        if (viewportHeight <= 0 || timelineList.contentHeight <= 0)
            return;

        if (!rootItem.perfLoggedContentHeightReady) {
            rootItem.perfLoggedContentHeightReady = true;
            rootItem.markRoomSwitchPerfPhase("qml.matrix_room.content_height_ready");
        }

        const usefulBufferedHeight = viewportHeight * 0.8;
        const desiredBufferedHeight = viewportHeight + Math.min(viewportHeight * 0.25, 320);
        const averageRowHeight = Math.max(56, Math.round(Settings.uiFontSizePt * 4.5));
        if (timelineList.contentHeight >= usefulBufferedHeight) {
            const wasRoomSwitchInProgress = rootItem.roomSwitchInProgress;
            if (!rootItem.perfLoggedUsefulHeightReady) {
                rootItem.perfLoggedUsefulHeightReady = true;
                rootItem.markRoomSwitchPerfPhase("qml.matrix_room.useful_height_ready");
            }
            rootItem.roomSwitchInProgress = false;
            if (wasRoomSwitchInProgress)
                support.scheduleReadMarkerUpdate(true);

            if (timelineList.contentHeight >= desiredBufferedHeight
                    || support.isInitialBufferCloseEnough(
                        viewportHeight,
                        timelineList.contentHeight,
                        averageRowHeight)) {
                if (!rootItem.perfLoggedBufferFilled) {
                    rootItem.perfLoggedBufferFilled = true;
                    rootItem.markRoomSwitchPerfPhase("qml.matrix_room.buffer_filled");
                }
                console.info("[timeline-load] Buffer filled: contentH="
                    + Math.round(timelineList.contentHeight)
                    + " desired=" + Math.round(desiredBufferedHeight)
                    + " count=" + (rootItem.perRoomModel ? rootItem.perRoomModel.count : 0));
                rootItem.initialTimelineBufferPending = false;
                rootItem.deferredInitialBufferTopUpPending = false;
                rootItem.bufferPaginationInFlight = false;
                rootItem.lastInitialBufferTriggerCount = -1;
                rootItem.deferredBufferCheckQueued = false;
                return;
            }

            rootItem.initialTimelineBufferPending = false;
            rootItem.deferredInitialBufferTopUpPending = true;
            support.scheduleDeferredInitialTimelineBufferCheck();
            return;
        }

        rootItem.bufferPaginationInFlight = false;

        const itemCount = (rootItem.perRoomModel ? rootItem.perRoomModel.count : 0);
        if (itemCount <= 0 || rootItem.lastInitialBufferTriggerCount === itemCount)
            return;

        const requestCount = Math.max(
            1,
            Math.min(12, Math.ceil((desiredBufferedHeight - timelineList.contentHeight) / averageRowHeight)));
        console.info("[timeline-load] Requesting buffer top-up: contentH="
            + Math.round(timelineList.contentHeight)
            + " desired=" + Math.round(desiredBufferedHeight)
            + " count=" + itemCount
            + " requesting=" + requestCount);
        if (!TimelineManager.paginateActiveMatrixTimelineBackwards(requestCount)) {
            rootItem.initialTimelineBufferPending = false;
            rootItem.bufferPaginationInFlight = false;
            rootItem.lastInitialBufferTriggerCount = -1;
            return;
        }

        rootItem.bufferPaginationInFlight = true;
        rootItem.lastInitialBufferTriggerCount = itemCount;
    }

    function estimatedInitialTimelinePageSize() {
        if (!timelineList)
            return 0;

        const viewportHeight = Number(timelineList.height || 0);
        if (viewportHeight <= 0)
            return 0;

        const desiredBufferedHeight = viewportHeight + Math.min(viewportHeight * 0.25, 320);
        const averageRowHeight = Math.max(56, Math.round(Settings.uiFontSizePt * 4.5));
        return Math.max(15, Math.min(50, Math.ceil(desiredBufferedHeight / averageRowHeight)));
    }

    function isInitialBufferCloseEnough(viewportHeight, contentHeight, averageRowHeight) {
        const desiredBufferedHeight = viewportHeight + Math.min(viewportHeight * 0.25, 320);
        const toleratedShortfall = Math.max(96, Math.min(averageRowHeight * 3, 240));
        return contentHeight >= desiredBufferedHeight - toleratedShortfall;
    }

    function updatePreferredInitialTimelinePageSize() {
        const pageSize = support.estimatedInitialTimelinePageSize();
        if (pageSize > 0)
            TimelineManager.setPreferredInitialMatrixTimelinePageSize(pageSize);
    }

    function maybeRequestDeferredInitialTimelineBuffer() {
        if (!timelineList
                || !rootItem.deferredInitialBufferTopUpPending
                || rootItem.initialBottomPinPending
                || rootItem.bufferPaginationInFlight
                || rootItem.loading
                || !rootItem.hasTimeline) {
            return;
        }

        const viewportHeight = timelineList.height;
        if (viewportHeight <= 0 || timelineList.contentHeight <= 0)
            return;

        const desiredBufferedHeight = viewportHeight + Math.min(viewportHeight * 0.25, 320);
        const averageRowHeight = Math.max(56, Math.round(Settings.uiFontSizePt * 4.5));
        if (timelineList.contentHeight >= desiredBufferedHeight
                || support.isInitialBufferCloseEnough(
                    viewportHeight,
                    timelineList.contentHeight,
                    averageRowHeight)) {
            if (!rootItem.perfLoggedBufferFilled) {
                rootItem.perfLoggedBufferFilled = true;
                rootItem.markRoomSwitchPerfPhase("qml.matrix_room.buffer_filled");
            }
            const wasRoomSwitchInProgress = rootItem.roomSwitchInProgress;
            rootItem.roomSwitchInProgress = false;
            if (wasRoomSwitchInProgress)
                support.scheduleReadMarkerUpdate(true);
            console.info("[timeline-load] Buffer filled: contentH="
                + Math.round(timelineList.contentHeight)
                + " desired=" + Math.round(desiredBufferedHeight)
                + " count=" + (rootItem.perRoomModel ? rootItem.perRoomModel.count : 0));
            rootItem.deferredInitialBufferTopUpPending = false;
            rootItem.bufferPaginationInFlight = false;
            rootItem.lastInitialBufferTriggerCount = -1;
            return;
        }

        rootItem.bufferPaginationInFlight = false;

        const itemCount = (rootItem.perRoomModel ? rootItem.perRoomModel.count : 0);
        if (itemCount <= 0 || rootItem.lastInitialBufferTriggerCount === itemCount)
            return;

        const requestCount = Math.max(
            1,
            Math.min(12, Math.ceil((desiredBufferedHeight - timelineList.contentHeight) / averageRowHeight)));
        console.info("[timeline-load] Requesting deferred buffer top-up: contentH="
            + Math.round(timelineList.contentHeight)
            + " desired=" + Math.round(desiredBufferedHeight)
            + " count=" + itemCount
            + " requesting=" + requestCount);
        if (!TimelineManager.paginateActiveMatrixTimelineBackwards(requestCount)) {
            rootItem.deferredInitialBufferTopUpPending = false;
            rootItem.bufferPaginationInFlight = false;
            rootItem.lastInitialBufferTriggerCount = -1;
            return;
        }

        rootItem.bufferPaginationInFlight = true;
        rootItem.lastInitialBufferTriggerCount = itemCount;
    }
}
