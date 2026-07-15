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

        // The user is scrolling; stop re-anchoring resets on a previous
        // event-jump target.
        rootItem.jumpAnchorEventId = "";

        // Track unpinned state during a scrollbar thumb drag.  Mirrors
        // handleWheelRotation: without this, keepPinnedToBottom stays
        // true (its initial value) and the next contentHeight change
        // (e.g. an offscreen delegate finishing layout) snaps the view
        // back to the bottom mid-drag.
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
    }

    function handleScrollbarReleased() {
        if (!timelineList)
            return;

        // Mirror wheelSettleTimer / handleMovementEnded: when the user
        // finishes a thumb drag at the live edge, treat it as a deliberate
        // return and re-pin so subsequent new messages auto-scroll.
        if (timelineList.isNearLiveEdge()) {
            timelineList.keepPinnedToBottom = true;
            timelineList.userUnpinned = false;
        }
        support.updateStableThumbSize();
    }

    function handleWheelRotation(rotation) {
        if (!timelineList)
            return;

        const delta = rotation - previousWheelRotation;
        previousWheelRotation = rotation;

        // If the content fits inside the viewport there is nothing to
        // scroll; bail out without touching contentY.  In BottomToTop
        // mode Qt anchors the items to the visual bottom, and forcing
        // contentY to originY would shift them upward away from that
        // anchor (regression observed on small thread timelines).
        const range = Math.max(0, timelineList.contentHeight - timelineList.height);
        if (range <= 0)
            return;

        // Clamp the target contentY ourselves before assignment.
        // Setting contentY out of bounds and then calling returnToBounds()
        // can let an over-scrolled value show on a render frame before the
        // snap-back lands, which manifests as visible jitter when the
        // user keeps wheeling at the top edge while pagination is in
        // flight (#79).
        const proposedContentY = timelineList.contentY - delta * 5;
        const minY = timelineList.originY;
        const maxY = timelineList.originY + range;
        timelineList.contentY = Math.max(minY, Math.min(maxY, proposedContentY));
        support.updateLastScroll();

        const halfW = Math.max(1, Math.round(timelineList.width / 2));
        const idx = timelineList.indexAt(halfW, timelineList.contentY + 2);
        if (idx >= 0)
            timelineList.savedTopIndex = idx;

        // The user is scrolling; stop re-anchoring resets on a previous
        // event-jump target.
        rootItem.jumpAnchorEventId = "";

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
                && !scrollbar.pressed
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
                && !scrollbar.pressed
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

    // Forcibly evict pooled and cached ListView delegates so the next round
    // of delegate creation has nothing to draw from and must rebind from
    // current model state.
    //
    // Qt's delegate reuse pool can retain delegates across model-instance
    // swaps (e.g. thread enter/leave) and across model resets within the
    // same model (the mixed-diff branch of
    // MatrixTimelineModel::replaceVisibleItems emits begin/endResetModel).
    // On reuse, Qt 6 sometimes leaves required properties pointing at the
    // previous row, surfacing as an "Unsupported message" fallback or as a
    // delegate that keeps its old eventId binding while the row beneath it
    // has moved on (duplicate bubble for one event, missing bubble for the
    // other). Toggling reuseItems off destroys the pool; zeroing
    // cacheBuffer also releases the offscreen stragglers that reuseItems
    // alone doesn't reach. Both restore on the next tick.
    function flushDelegateReusePool() {
        if (!timelineList)
            return;

        if (timelineList.reuseItems) {
            timelineList.reuseItems = false;
            Qt.callLater(function () {
                if (timelineList)
                    timelineList.reuseItems = true;
            });
        }

        const savedCacheBuffer = timelineList.cacheBuffer;
        if (savedCacheBuffer > 0) {
            timelineList.cacheBuffer = 0;
            Qt.callLater(function () {
                if (timelineList)
                    timelineList.cacheBuffer = savedCacheBuffer;
            });
        }
    }

    function handleModelChanged() {
        if (!timelineList)
            return;

        flushDelegateReusePool();

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

        // Only reset the no-progress guard when we observe actual count
        // movement since the last buffer top-up trigger. Without this
        // gate, a contentHeight oscillation that doesn't reflect a real
        // model change (e.g. the loading spinner footer expanding then
        // collapsing on each paginate cycle) re-arms the guard and the
        // deferred top-up retriggers paginate forever in rooms whose
        // history has truly run out (#79 follow-up).
        const currentCount = rootItem.perRoomModel ? rootItem.perRoomModel.count : 0;
        const currentRawCount = rootItem.perRoomModel ? rootItem.perRoomModel.rawCount : 0;
        const countMoved = rootItem.lastInitialBufferTriggerCount >= 0
            && (rootItem.lastInitialBufferTriggerCount !== currentCount
                || rootItem.lastInitialBufferTriggerRawCount !== currentRawCount);
        if (countMoved || rootItem.lastInitialBufferTriggerCount < 0) {
            rootItem.bufferPaginationInFlight = false;
            rootItem.lastInitialBufferTriggerCount = -1;
            rootItem.lastInitialBufferTriggerRawCount = -1;
        }

        // The whole timeline already fits inside the viewport, so the room
        // switch is effectively done — content is on screen and the user
        // can read it.  Clear roomSwitchInProgress and schedule a read
        // marker update; otherwise small rooms get stuck with the unread
        // badge because the deferred top-up only clears that flag once
        // contentHeight reaches desiredBufferedHeight, which is
        // unreachable when the room has fewer messages than the viewport
        // can hold.
        if (rootItem.roomSwitchInProgress) {
            if (!rootItem.perfLoggedUsefulHeightReady) {
                rootItem.perfLoggedUsefulHeightReady = true;
                rootItem.markRoomSwitchPerfPhase("qml.matrix_room.useful_height_ready");
            }
            rootItem.roomSwitchInProgress = false;
            rootItem.scheduleReadMarkerUpdate(true);
        }
    }

    function handleModelResetAboutToReplace() {
        savedResetEventId = "";
        savedResetWasPinnedToBottom = true;

        // Fires before MatrixTimelineModel::beginResetModel(), so toggling
        // reuseItems off here clears the pool before Qt populates it with
        // the soon-to-be-reset visible delegates. Without this, a row that
        // changes identity across the reset (a local echo gaining its real
        // event_id, or a mixed insert+remove diff that lands on the reset
        // path) can come out of reuse still bound to its previous row.
        flushDelegateReusePool();

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

        // An event-jump target takes over as the restore anchor: trailing
        // pagination/receipt snapshots keep resetting the model for a
        // while after a jump lands, and the nearest-center anchor drifts
        // once newly loaded items with different heights land around the
        // viewport. Re-centering on the target keeps the jump visually
        // stable until the user takes over scrolling.
        var jumpTarget = String(rootItem.jumpAnchorEventId || "");

        if (savedResetEventId.length === 0 && jumpTarget.length === 0)
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

            var anchor = jumpTarget.length > 0 ? jumpTarget : eid;
            var row = model.rowForEventId(anchor);
            if (row < 0 && anchor !== eid && eid.length > 0) {
                anchor = eid;
                row = model.rowForEventId(anchor);
            }
            if (row < 0 || row >= timelineList.count)
                return;

            timelineList.positionViewAtIndex(row, ListView.Center);
            timelineList.returnToBounds();
            updateLastScroll();

            // The reset recreated the target's delegate, which swallows
            // the one-shot highlight-flash transition mid-run. Re-trigger
            // it so the flash the user actually sees is the one that
            // completes (and clears highlightedEventId via eventShown).
            if (anchor === jumpTarget && rootItem.highlightedEventId === jumpTarget) {
                rootItem.highlightedEventId = "";
                Qt.callLater(function () {
                    if (rootItem.highlightedEventId.length === 0)
                        rootItem.highlightedEventId = jumpTarget;
                });
            }
        });
    }

    function resetForRoomSwitch() {
        previousWheelRotation = 0;
        wheelSettleTimer.stop();
        savedResetEventId = "";
        savedResetWasPinnedToBottom = true;
        rootItem.jumpAnchorEventId = "";
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
