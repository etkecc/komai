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
        rootItem.updatePreferredInitialTimelinePageSize();
        rootItem.measuredTimelineHeights = ({});
        rootItem.roomSwitchInProgress = rootItem.activeRoomId.length > 0;
        rootItem.initialBottomPinPending = rootItem.activeRoomId.length > 0;
        rootItem.initialTimelineBufferPending = rootItem.activeRoomId.length > 0;
        rootItem.deferredInitialBufferTopUpPending = false;
        rootItem.bufferPaginationInFlight = false;
        rootItem.perfLoggedCountNonZero = false;
        rootItem.perfLoggedContentHeightReady = false;
        rootItem.perfLoggedUsefulHeightReady = false;
        rootItem.perfLoggedBufferFilled = false;
        rootItem.readMarkerGeneration += 1;
        rootItem.preferLatestReadMarkerEvent = false;
        rootItem.lastMarkedReadEventId = "";
        rootItem.lastInitialBufferTriggerCount = -1;
        rootItem.pendingComposerAutoFocus = rootItem.activeRoomId.length > 0;
        rootItem.visibleTimelineDelegates = ({});
        rootItem.deferredBufferCheckGeneration += 1;
        rootItem.deferredBufferCheckQueued = false;

        if (rootItem.activeRoomId.length > 0)
            support.markRoomSwitchPerfPhase("qml.matrix_room.active_room_changed");
        if (rootItem.pendingComposerAutoFocus)
            rootItem.scheduleComposerAutoFocus();

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

    property var rootConnections: Connections {
        target: rootItem

        function onActiveRoomIdChanged() {
            support.handleActiveRoomIdChanged();
        }

        function onLoadingChanged() {
            support.handleLoadingChanged();
        }
    }
}
