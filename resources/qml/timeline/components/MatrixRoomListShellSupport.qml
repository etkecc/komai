// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import cc.etke.komai

QtObject {
    id: support

    required property var rootItem
    required property var timelineList
    required property var scrollbar

    property real previousWheelRotation: 0

    property var wheelSettleTimer: Timer {
        interval: 250
        onTriggered: {
            if (!support.timelineList.userUnpinned)
                support.timelineList.keepPinnedToBottom = support.timelineList.atYEnd;
            support.updateStableThumbSize();
        }
    }

    function updateStableThumbSize() {
        if (!timelineList || timelineList.count <= 0 || timelineList.height <= 0
                || timelineList.contentHeight <= timelineList.height) {
            return;
        }

        const newSize = Math.max(0.02, timelineList.height / timelineList.contentHeight);
        if (!timelineList.visibleIndicesValid || newSize < timelineList.stableThumbSize) {
            timelineList.stableThumbSize = newSize;
            timelineList.visibleIndicesValid = true;
        }
    }

    function updateLastScroll() {
        if (!timelineList)
            return;

        timelineList.lastScrollPos = timelineList.contentY + timelineList.height;
    }

    function updateBottomPin() {
        if (!timelineList)
            return;

        if (rootItem.initialBottomPinPending) {
            timelineList.keepPinnedToBottom = true;
            if (timelineList.atYEnd) {
                rootItem.initialBottomPinPending = false;
                rootItem.scheduleInitialTimelineBufferCheck();
            }
            return;
        }

        timelineList.keepPinnedToBottom = timelineList.atYEnd;
    }

    function maybeScrollToBottom(force) {
        if (!timelineList || timelineList.count <= 0 || timelineList.userUnpinned)
            return;

        if (!(force || timelineList.keepPinnedToBottom || rootItem.initialBottomPinPending))
            return;

        Qt.callLater(function () {
            if (!timelineList || timelineList.count <= 0 || timelineList.userUnpinned
                    || !(force || timelineList.keepPinnedToBottom || rootItem.initialBottomPinPending)) {
                return;
            }

            timelineList.positionViewAtBeginning();
            support.updateBottomPin();
        });
    }

    function handleScrollbarPositionChanged() {
        if (!timelineList || !scrollbar || !scrollbar.pressed)
            return;

        if (scrollbar.positionOnPress >= 0) {
            const saved = scrollbar.positionOnPress;
            scrollbar.positionOnPress = -1;
            if (Math.abs(scrollbar.position - saved) > 0.01) {
                scrollbar.position = saved;
                return;
            }
        }

        const ch = timelineList.contentHeight;
        const h = timelineList.height;
        const range = ch - h;
        if (range <= 0)
            return;

        const maxPos = 1.0 - scrollbar.size;
        if (maxPos <= 0)
            return;

        const normalized = scrollbar.position / maxPos;
        timelineList.contentY = timelineList.originY + normalized * range;
        timelineList.returnToBounds();
        support.updateLastScroll();
    }

    function handleWheelRotation(rotation) {
        if (!timelineList)
            return;

        const delta = rotation - previousWheelRotation;
        previousWheelRotation = rotation;
        timelineList.contentY -= delta * 5;
        timelineList.returnToBounds();
        support.updateLastScroll();

        const halfW = Math.max(1, Math.round(timelineList.width / 2));
        const idx = timelineList.indexAt(halfW, timelineList.contentY + 2);
        if (idx >= 0)
            timelineList.savedTopIndex = idx;

        if (!timelineList.atYEnd) {
            timelineList.keepPinnedToBottom = false;
            timelineList.userUnpinned = true;
            if (rootItem.initialBottomPinPending)
                rootItem.initialBottomPinPending = false;
            if (rootItem.initialTimelineBufferPending)
                rootItem.initialTimelineBufferPending = false;
            if (rootItem.deferredInitialBufferTopUpPending)
                rootItem.deferredInitialBufferTopUpPending = false;
            rootItem.deferredBufferCheckGeneration += 1;
            rootItem.deferredBufferCheckQueued = false;
        }

        wheelSettleTimer.restart();
    }

    function handleMovementEnded() {
        if (!timelineList)
            return;

        support.updateLastScroll();
        timelineList.keepPinnedToBottom = timelineList.atYEnd;
        if (timelineList.atYEnd)
            timelineList.userUnpinned = false;
        rootItem.scheduleReadMarkerUpdate(timelineList.atYEnd);
        support.updateStableThumbSize();
    }

    function handleAtYBeginningChanged() {
        if (!timelineList || !timelineList.atYBeginning || !rootItem.hasTimeline || rootItem.loading
                || !timelineList.userUnpinned || rootItem.initialTimelineBufferPending
                || rootItem.lastPaginationTriggerCount === TimelineManager.matrixTimelineItemCount) {
            return;
        }

        console.info("[timeline-load] Scroll-triggered pagination at top, count="
            + TimelineManager.matrixTimelineItemCount);
        if (TimelineManager.paginateActiveMatrixTimelineBackwards(0))
            rootItem.lastPaginationTriggerCount = TimelineManager.matrixTimelineItemCount;
    }

    function handleContentYChanged() {
        if (!timelineList)
            return;

        if (!timelineList.atYBeginning
                && rootItem.lastPaginationTriggerCount === TimelineManager.matrixTimelineItemCount) {
            rootItem.lastPaginationTriggerCount = -1;
        }

        if ((timelineList.moving || timelineList.flicking || timelineList.dragging)
                && !timelineList.atYEnd) {
            if (rootItem.initialBottomPinPending)
                rootItem.initialBottomPinPending = false;
            if (rootItem.initialTimelineBufferPending)
                rootItem.initialTimelineBufferPending = false;
            if (rootItem.deferredInitialBufferTopUpPending)
                rootItem.deferredInitialBufferTopUpPending = false;
            rootItem.deferredBufferCheckGeneration += 1;
            rootItem.deferredBufferCheckQueued = false;
        }
    }

    function handleContentHeightChanged() {
        if (!timelineList || !scrollbar)
            return;

        if (scrollbar.pressed) {
            const range = timelineList.contentHeight - timelineList.height;
            if (range > 0) {
                const normalized = (timelineList.contentY - timelineList.originY) / range;
                const maxPos = 1.0 - scrollbar.size;
                scrollbar.position = Math.max(0, Math.min(maxPos, normalized * maxPos));
            }
        }

        if (!timelineList.moving && !timelineList.flicking && !timelineList.dragging
                && !timelineList.userUnpinned) {
            if (timelineList.keepPinnedToBottom || rootItem.initialBottomPinPending) {
                timelineList.positionViewAtBeginning();
                support.updateBottomPin();
            } else {
                support.maybeScrollToBottom(timelineList.previousCount === 0);
            }
            support.updateLastScroll();
        }

        if (rootItem.deferredInitialBufferTopUpPending)
            rootItem.scheduleDeferredInitialTimelineBufferCheck();
        else
            rootItem.scheduleInitialTimelineBufferCheck();
    }

    function handleHeightChanged() {
        if (!timelineList)
            return;

        rootItem.updatePreferredInitialTimelinePageSize();
        if (!timelineList.moving && !timelineList.flicking && !timelineList.dragging
                && !timelineList.userUnpinned) {
            if (timelineList.keepPinnedToBottom || rootItem.initialBottomPinPending) {
                timelineList.positionViewAtBeginning();
                support.updateBottomPin();
            } else {
                timelineList.contentY = timelineList.lastScrollPos - timelineList.height;
                support.maybeScrollToBottom(timelineList.previousCount === 0);
            }
            support.updateLastScroll();
        }

        if (rootItem.deferredInitialBufferTopUpPending)
            rootItem.scheduleDeferredInitialTimelineBufferCheck();
        else
            rootItem.scheduleInitialTimelineBufferCheck();
    }

    function handleCountChanged() {
        if (!timelineList)
            return;

        if (timelineList.count !== timelineList.previousCount)
            rootItem.bufferPaginationInFlight = false;

        if (timelineList.count > 0 && !rootItem.perfLoggedCountNonZero) {
            rootItem.perfLoggedCountNonZero = true;
            rootItem.markRoomSwitchPerfPhase("qml.matrix_room.count_nonzero");
        }

        const forceScroll = timelineList.previousCount === 0 && !timelineList.visibleIndicesValid;
        if (!timelineList.userUnpinned
                && (forceScroll || timelineList.keepPinnedToBottom || rootItem.initialBottomPinPending)) {
            timelineList.positionViewAtBeginning();
            support.updateBottomPin();
        } else {
            support.maybeScrollToBottom(forceScroll);
        }

        support.updateLastScroll();
        Qt.callLater(function () {
            support.updateStableThumbSize();
        });
        if (rootItem.deferredInitialBufferTopUpPending)
            rootItem.scheduleDeferredInitialTimelineBufferCheck();
        else
            rootItem.scheduleInitialTimelineBufferCheck();
        rootItem.scheduleReadMarkerUpdate(!timelineList.userUnpinned
            && (forceScroll || timelineList.keepPinnedToBottom || rootItem.initialBottomPinPending
                || timelineList.atYEnd));
        timelineList.previousCount = timelineList.count;
    }

    function handleModelChanged() {
        if (!timelineList)
            return;

        timelineList.previousCount = timelineList.count;
        if (!timelineList.userUnpinned && timelineList.keepPinnedToBottom && timelineList.count > 0)
            timelineList.positionViewAtBeginning();
        support.updateLastScroll();
    }

    function handleCompleted() {
        if (!timelineList)
            return;

        timelineList.previousCount = timelineList.count;
        support.updateLastScroll();
        rootItem.updatePreferredInitialTimelinePageSize();
        support.maybeScrollToBottom(true);
    }

    function resetForRoomSwitch() {
        previousWheelRotation = 0;
        wheelSettleTimer.stop();
        if (!timelineList)
            return;

        timelineList.keepPinnedToBottom = true;
        timelineList.userUnpinned = false;
        timelineList.savedTopIndex = -1;
        timelineList.previousCount = 0;
        timelineList.visibleIndicesValid = false;
        timelineList.stableThumbSize = 1.0;
    }
}
