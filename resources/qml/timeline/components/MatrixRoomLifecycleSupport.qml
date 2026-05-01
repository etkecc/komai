// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import cc.etke.komai

QtObject {
    id: support

    required property var rootItem
    required property var topBar
    required property var listShellSupport
    required property var viewportSupport

    function clearSearch() {
        topBar.searchString = "";
    }

    function markRoomSwitchPerfPhase(phase) {
        if (!TimelineManager.roomSwitchPerfEnabled()
                || rootItem.activeRoomId.length === 0
                || phase.length === 0) {
            return;
        }

        TimelineManager.markRoomSwitchPhase(rootItem.activeRoomId, phase);
    }

    function handleActiveRoomIdChanged() {
        if (!rootItem.poolActive)
            return;
        _resetForRoom(true, false);
    }

    // Lightweight reactivation for pool entries returning to the foreground.
    // Reconnects viewport and model state but preserves walk mode, selection,
    // and other user-interaction state.  When preserveScroll is true (tab
    // switch back to an already-open tab), scroll position and buffer state
    // are kept intact instead of resetting to the bottom.
    function handlePoolReactivation(preserveScroll) {
        _resetForRoom(false, !!preserveScroll);
        // Pool reactivation doesn't trigger onVisibleChanged (the item is
        // already visible when poolActive becomes true), so explicitly
        // schedule a read marker update for unread messages on tab switch.
        if (rootItem.visible
                && rootItem.activeRoomId.length > 0
                && !rootItem.roomSwitchInProgress) {
            viewportSupport.scheduleReadMarkerUpdate(true);
        }
    }

    function _resetForRoom(fullReset, preserveScroll) {
        if (!preserveScroll) {
            rootItem.updatePreferredInitialTimelinePageSize();
            rootItem.measuredTimelineHeights = ({});
            rootItem.roomSwitchInProgress = rootItem.activeRoomId.length > 0;
            rootItem.initialBottomPinPending = rootItem.activeRoomId.length > 0;
            rootItem.initialTimelineBufferPending = rootItem.activeRoomId.length > 0;
            rootItem.deferredInitialBufferTopUpPending = false;
            rootItem.bufferPaginationInFlight = false;
            rootItem.lastInitialBufferTriggerCount = -1;
            rootItem.lastInitialBufferTriggerRawCount = -1;
            rootItem.deferredBufferCheckQueued = false;
            rootItem.paginationProgresslessAttempts = 0;
        }

        rootItem.perfLoggedCountNonZero = false;
        rootItem.perfLoggedContentHeightReady = false;
        rootItem.perfLoggedUsefulHeightReady = false;
        rootItem.perfLoggedBufferFilled = false;
        rootItem.readMarkerGeneration += 1;
        rootItem.preferLatestReadMarkerEvent = false;
        rootItem.lastMarkedReadEventId = "";
        rootItem.pendingComposerAutoFocus = rootItem.activeRoomId.length > 0;
        rootItem._composerAutoFocusRetries = 0;
        rootItem.visibleTimelineDelegates = ({});
        rootItem.delegateRegistrationGeneration += 1;

        if (fullReset) {
            rootItem.walkModeActive = false;
            rootItem.focusedEventId = "";
            rootItem.selectedEventIds = [];
            rootItem.selectionAnchorEventId = "";
            rootItem.suppressNextWalkModeOlderStep = false;
        }

        if (rootItem.activeRoomId.length > 0)
            support.markRoomSwitchPerfPhase(fullReset
                ? "qml.matrix_room.active_room_changed"
                : "qml.matrix_room.pool_reactivated");
        if (rootItem.pendingComposerAutoFocus)
            rootItem.scheduleComposerAutoFocus();

        if (!preserveScroll)
            listShellSupport.resetForRoomSwitch();
        viewportSupport.resetReadMarkerState();
    }

    function handleLoadingChanged() {
        if (rootItem.loading)
            return;

        if (rootItem.pendingComposerAutoFocus)
            rootItem.scheduleComposerAutoFocus();

        if (!rootItem.hasTimeline)
            rootItem.roomSwitchInProgress = false;

        support.markRoomSwitchPerfPhase("qml.matrix_room.loading_false");
        rootItem.ensureInitialBottomPin();
        if (rootItem.deferredInitialBufferTopUpPending)
            rootItem.scheduleDeferredInitialTimelineBufferCheck();
        else
            rootItem.scheduleInitialTimelineBufferCheck();
    }

    // activeRoomId may already be set when this component is created (the binding
    // on the parent evaluates before children exist), so the onActiveRoomIdChanged
    // signal fires before the Connections block below is wired up.  Re-run the
    // handler once on completion to cover that missed signal.
    Component.onCompleted: {
        if (rootItem.poolActive && rootItem.activeRoomId.length > 0)
            support.handleActiveRoomIdChanged();
    }

    property var rootConnections: Connections {
        target: rootItem

        function onActiveRoomIdChanged() {
            support.handleActiveRoomIdChanged();
        }

        function onLoadingChanged() {
            if (!rootItem.poolActive)
                return;
            support.handleLoadingChanged();
        }

        function onVisibleChanged() {
            if (!rootItem.poolActive)
                return;
            if (rootItem.visible
                    && rootItem.activeRoomId.length > 0
                    && !rootItem.roomSwitchInProgress
                    && rootItem.lastMarkedReadEventId.length === 0) {
                viewportSupport.scheduleReadMarkerUpdate(true);
            }
        }
    }
}
