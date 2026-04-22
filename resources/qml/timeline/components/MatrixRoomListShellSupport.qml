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

    // Scroll position state saved before a model reset (beginResetModel
    // in BottomToTop mode resets contentY).  Restored in
    // handleModelResetContentReplaced() after the reset completes.
    property string savedResetEventId: ""
    property bool savedResetWasPinnedToBottom: true

    property var wheelSettleTimer: Timer {
        interval: 250
        onTriggered: {
            // If the wheel came to rest near the bottom, treat it as a
            // deliberate return to the live edge — re-pin and clear the
            // userUnpinned flag so new messages auto-scroll again.
            if (support.timelineList.isNearLiveEdge()) {
                support.timelineList.keepPinnedToBottom = true;
                support.timelineList.userUnpinned = false;
            } else if (!support.timelineList.userUnpinned) {
                support.timelineList.keepPinnedToBottom = false;
            }
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
            if (timelineList.isNearLiveEdge()) {
                rootItem.initialBottomPinPending = false;
                rootItem.scheduleInitialTimelineBufferCheck();
            }
            return;
        }

        timelineList.keepPinnedToBottom = timelineList.isNearLiveEdge();
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

        if (!timelineList.isNearLiveEdge()) {
            timelineList.keepPinnedToBottom = false;
            timelineList.userUnpinned = true;
            if (rootItem.initialBottomPinPending)
                rootItem.initialBottomPinPending = false;
            if (rootItem.initialTimelineBufferPending)
                rootItem.initialTimelineBufferPending = false;
            if (rootItem.deferredInitialBufferTopUpPending)
                rootItem.deferredInitialBufferTopUpPending = false;
            rootItem.deferredBufferCheckQueued = false;
        }

        wheelSettleTimer.restart();
    }

    function handleMovementEnded() {
        if (!timelineList)
            return;

        support.updateLastScroll();
        const atEdge = timelineList.isNearLiveEdge();
        timelineList.keepPinnedToBottom = atEdge;
        if (atEdge)
            timelineList.userUnpinned = false;
        rootItem.scheduleReadMarkerUpdate(atEdge);
        support.updateStableThumbSize();
    }

    function handleAtYBeginningChanged() {
        if (!timelineList || !timelineList.atYBeginning || !rootItem.hasTimeline || rootItem.loading
                || !timelineList.userUnpinned || rootItem.initialTimelineBufferPending
                || rootItem.lastPaginationTriggerCount === (rootItem.perRoomModel ? rootItem.perRoomModel.count : 0)) {
            return;
        }

        console.info("[timeline-load] Scroll-triggered pagination at top, count="
            + (rootItem.perRoomModel ? rootItem.perRoomModel.count : 0));
        if (TimelineManager.paginateActiveMatrixTimelineBackwards(0))
            rootItem.lastPaginationTriggerCount = (rootItem.perRoomModel ? rootItem.perRoomModel.count : 0);
    }

    function handleContentYChanged() {
        if (!timelineList)
            return;

        if (!timelineList.atYBeginning
                && rootItem.lastPaginationTriggerCount === (rootItem.perRoomModel ? rootItem.perRoomModel.count : 0)) {
            rootItem.lastPaginationTriggerCount = -1;
        }

        if ((timelineList.moving || timelineList.flicking || timelineList.dragging)
                && !timelineList.isNearLiveEdge()) {
            if (rootItem.initialBottomPinPending)
                rootItem.initialBottomPinPending = false;
            if (rootItem.initialTimelineBufferPending)
                rootItem.initialTimelineBufferPending = false;
            if (rootItem.deferredInitialBufferTopUpPending)
                rootItem.deferredInitialBufferTopUpPending = false;
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

        support.armDeferredBufferTopUpIfUnderfilled();

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

        support.armDeferredBufferTopUpIfUnderfilled();

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

        if (timelineList.count > 0 && rootItem.pendingComposerAutoFocus)
            rootItem.scheduleComposerAutoFocus();

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

        support.armDeferredBufferTopUpIfUnderfilled();

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

        // Qt's delegate reuse pool retains delegates across model-instance
        // swaps (e.g. entering/leaving thread view). On reuse for the new
        // model it can leave required properties pointing at the previous
        // model's row, which surfaces as "Unsupported message" for events
        // that live in the old model but not the new one. Briefly toggling
        // reuseItems destroys the pooled items so new reuses re-read roles
        // from the current model.
        if (timelineList.reuseItems) {
            timelineList.reuseItems = false;
            Qt.callLater(function () {
                if (timelineList)
                    timelineList.reuseItems = true;
            });
        }

        // Cached-item flush: offscreen delegates sitting in the cacheBuffer
        // region aren't always released on model swap, so stragglers from
        // the old model can keep rendering alongside new-model items (and
        // their EventDelegateChooser.room binding flips under them on a
        // thread↔per-room swap). Zeroing cacheBuffer forces release of
        // those cached items; restore on the next tick.
        const savedCacheBuffer = timelineList.cacheBuffer;
        if (savedCacheBuffer > 0) {
            timelineList.cacheBuffer = 0;
            Qt.callLater(function () {
                if (timelineList)
                    timelineList.cacheBuffer = savedCacheBuffer;
            });
        }

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

    function armDeferredBufferTopUpIfUnderfilled() {
        if (!timelineList
                || timelineList.count <= 0
                || rootItem.loading
                || rootItem.initialBottomPinPending) {
            return;
        }

        const viewportHeight = Number(timelineList.height || 0);
        const contentHeight = Number(timelineList.contentHeight || 0);
        if (viewportHeight <= 0 || contentHeight <= 0 || contentHeight > viewportHeight + 1)
            return;

        rootItem.initialTimelineBufferPending = false;
        rootItem.deferredInitialBufferTopUpPending = true;
        rootItem.bufferPaginationInFlight = false;
        rootItem.lastInitialBufferTriggerCount = -1;
    }

    function handleModelResetAboutToReplace() {
        savedResetEventId = "";
        savedResetWasPinnedToBottom = true;

        if (!timelineList || timelineList.count <= 0)
            return;

        savedResetWasPinnedToBottom = timelineList.keepPinnedToBottom && !timelineList.userUnpinned;
        if (savedResetWasPinnedToBottom)
            return;

        // Find the event ID of the item nearest the viewport center so we
        // can restore the scroll position after the model reset.
        var model = timelineList.model;
        if (!model || typeof model.itemAt !== "function")
            return;

        var centerY = timelineList.contentY + timelineList.height / 2;
        var halfW = Math.max(1, Math.round(timelineList.width / 2));
        var idx = timelineList.indexAt(halfW, centerY);

        // indexAt can return -1 during layout transitions; fall back to
        // the bottom-visible index (index 0 in BottomToTop is at the
        // visual bottom).
        if (idx < 0)
            idx = timelineList.indexAt(halfW, timelineList.contentY + 2);
        if (idx < 0 || idx >= timelineList.count)
            return;

        var item = model.itemAt(idx);
        if (item) {
            var eid = item["eventId"] || "";
            if (eid.length > 0)
                savedResetEventId = eid;
        }
    }

    function handleModelResetContentReplaced() {
        if (!timelineList)
            return;

        if (savedResetWasPinnedToBottom) {
            timelineList.positionViewAtBeginning();
            updateBottomPin();
            updateLastScroll();
            savedResetEventId = "";
            return;
        }

        if (savedResetEventId.length === 0)
            return;

        var eid = savedResetEventId;
        savedResetEventId = "";

        // Defer positioning to the next frame so the ListView has
        // finished laying out delegates from the new model data.
        Qt.callLater(function () {
            if (!timelineList || !timelineList.model)
                return;

            var model = timelineList.model;
            if (typeof model.rowForEventId !== "function")
                return;

            var row = model.rowForEventId(eid);
            if (row < 0 || row >= timelineList.count)
                return;

            timelineList.positionViewAtIndex(row, ListView.Center);
            timelineList.returnToBounds();
            updateLastScroll();
        });
    }

    function resetForRoomSwitch() {
        previousWheelRotation = 0;
        wheelSettleTimer.stop();
        savedResetEventId = "";
        savedResetWasPinnedToBottom = true;
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
