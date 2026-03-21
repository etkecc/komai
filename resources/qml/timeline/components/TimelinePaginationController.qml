// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import cc.etke.komai

Item {
    id: root

    required property var chatList
    required property var scrollbar
    required property var activeRoomModel
    required property var roomModel
    required property bool disableTimelineList
    required property bool filteringRequested
    required property bool roomSwitchInProgress

    // Automatic underfill top-ups are bounded to avoid giant first expansion jumps.
    // Once the user scrolls, top-pagination remains unrestricted.
    property int autoExpandBudgetPerBind: 2
    property int autoExpandRemaining: 2
    property bool userInteractedSinceBind: false
    property bool autoTopupAllowed: false
    property bool topPaginationRequestedForCurrentTop: false
    property bool pendingTopPaginationAfterInteraction: false
    // Use an absolute pixel proximity to avoid percentage drift as the window grows.
    property real nearTopThresholdPx: 180
    // After search closes, suppress automatic pagination until the user
    // intentionally returns to timeline-top. This prevents immediate
    // post-search expansion bursts on a huge window.
    property bool suppressPaginationUntilUserScroll: false
    // Throttle back-to-back requestMore() calls; virtual-window expansion is
    // synchronous and can otherwise chain aggressively under drag/scroll.
    property bool paginationCooldownActive: false
    property int paginationCooldownMs: 80
    // Search may expand the room window deeply. Reset it once on search exit so
    // normal timeline interactions don't carry a 10k+ row window cost.
    property bool pendingWindowResetAfterSearch: false

    onFilteringRequestedChanged: {
        if (!filteringRequested) {
            // Search deactivated: switch back to normal timeline behavior with
            // guarded pagination and a one-shot window reset.
            suppressPaginationUntilUserScroll = true;
            pendingWindowResetAfterSearch = true;
            maybeResetWindowAfterSearch();
        }
    }

    onRoomSwitchInProgressChanged: {
        if (!roomSwitchInProgress) {
            underfillTopupDelay.start();
            scheduleNeededPagination();
        }
    }

    Connections {
        target: root.activeRoomModel

        function onPaginationInProgressChanged() {
            if (!root.activeRoomModel)
                return;
            root.maybeResetWindowAfterSearch();
            if (!root.activeRoomModel.paginationInProgress)
                root.scheduleNeededPagination();
        }

        function onFetchedMore() {
            if (!root.activeRoomModel)
                return;
            root.scheduleNeededPagination();
        }
    }

    Connections {
        target: root.scrollbar

        function onPressedChanged() {
            if (!root.scrollbar)
                return;

            if (root.scrollbar.pressed) {
                root.userInteractedSinceBind = true;
                return;
            }

            root.maybeFlushPendingTopPagination();
        }
    }

    Timer {
        id: neededPaginationTimer

        interval: 0
        repeat: false
        onTriggered: root.maybeRequestMoreIfNeeded()
    }

    Timer {
        id: underfillTopupDelay

        interval: 220
        repeat: false
        onTriggered: {
            root.autoTopupAllowed = true;
            root.scheduleNeededPagination();
        }
    }

    Timer {
        id: paginationCooldown

        interval: root.paginationCooldownMs
        repeat: false
        onTriggered: {
            root.paginationCooldownActive = false;
            root.scheduleNeededPagination();
        }
    }

    function markPhase(phase) {
        if (TimelineManager.roomSwitchPerfEnabled() && roomModel)
            TimelineManager.markRoomSwitchPhase(roomModel.roomId, phase);
    }

    function onBindCompleted() {
        userInteractedSinceBind = false;
        autoTopupAllowed = false;
        autoExpandRemaining = autoExpandBudgetPerBind;
        topPaginationRequestedForCurrentTop = false;
        pendingTopPaginationAfterInteraction = false;
        paginationCooldownActive = false;
        suppressPaginationUntilUserScroll = false;
        pendingWindowResetAfterSearch = false;
        paginationCooldown.stop();
        neededPaginationTimer.restart();
    }

    function onMovementStarted() {
        userInteractedSinceBind = true;
        resetTopPaginationLatchIfAwayFromTop();
    }

    function onMovementEnded() {
        if (!chatList)
            return;
        if (chatList.atYBeginning)
            suppressPaginationUntilUserScroll = false;
        resetTopPaginationLatchIfAwayFromTop();
        maybeFlushPendingTopPagination();
        if (chatList.atYBeginning && !pendingTopPaginationAfterInteraction)
            scheduleNeededPagination();
    }

    function onAtYBeginningChanged(atYBeginning) {
        if (!atYBeginning) {
            topPaginationRequestedForCurrentTop = false;
        }
        if (atYBeginning && chatList && !roomSwitchInProgress &&
            (chatList.contentHeight > chatList.height + 1)) {
            // Dragging the scrollbar thumb to top may not report movement-start.
            // Treat entering top-edge on a scrollable list as user interaction.
            userInteractedSinceBind = true;
            if (isInteractionActive())
                armPendingTopPaginationAfterInteraction();
        }
        if (atYBeginning && !isInteractionActive())
            scheduleNeededPagination();
        else if (atYBeginning)
            armPendingTopPaginationAfterInteraction();
    }

    function onTimelineModelChanged() {
        scheduleNeededPagination();
    }

    function onCountChanged() {
        scheduleNeededPagination();
    }

    function triggerRequestMore(phase) {
        const model = activeRoomModel;

        if (!model || !chatList || disableTimelineList || filteringRequested || roomSwitchInProgress)
            return false;
        if (model.paginationInProgress)
            return false;
        if (paginationCooldownActive)
            return false;
        if (!model.canPaginateBack()) {
            markPhase(phase + ".skip.no_more");
            return false;
        }

        markPhase(phase);
        model.requestMore();
        paginationCooldownActive = true;
        paginationCooldown.restart();
        return true;
    }

    function requestMoreForOlderKeyboardWalk() {
        userInteractedSinceBind = true;
        suppressPaginationUntilUserScroll = false;
        pendingTopPaginationAfterInteraction = false;

        if (chatList)
            chatList.keepPinnedToBottom = false;

        return triggerRequestMore("qml.message_view.expand_if_needed.request.keyboard_walk");
    }

    function scheduleNeededPagination() {
        if (!neededPaginationTimer.running)
            neededPaginationTimer.start();
    }

    function isInteractionActive() {
        return !!chatList && (chatList.moving || chatList.flicking || chatList.dragging ||
               (!!scrollbar && scrollbar.pressed));
    }

    function maybeFlushPendingTopPagination() {
        if (!pendingTopPaginationAfterInteraction)
            return;
        if (isInteractionActive())
            return;

        if (!isNearTopZone()) {
            pendingTopPaginationAfterInteraction = false;
            return;
        }

        // Keep pending=true until a successful request path consumes it.
        scheduleNeededPagination();
    }

    function resetTopPaginationLatchIfAwayFromTop() {
        if (!topPaginationRequestedForCurrentTop)
            return;
        if (!isNearTopZone())
            topPaginationRequestedForCurrentTop = false;
    }

    function armPendingTopPaginationAfterInteraction() {
        pendingTopPaginationAfterInteraction = true;
    }

    function isNearTopZone() {
        if (!chatList)
            return false;
        if (chatList.atYBeginning)
            return true;
        return Math.abs(chatList.contentY - chatList.originY) <= nearTopThresholdPx;
    }

    function maybeRequestMoreIfNeeded() {
        const model = activeRoomModel;
        if (!model || !chatList || disableTimelineList || filteringRequested || roomSwitchInProgress)
            return;
        resetTopPaginationLatchIfAwayFromTop();
        if (model.paginationInProgress)
            return;
        if (paginationCooldownActive)
            return;

        const underfilled = chatList.contentHeight <= chatList.height + 1;
        const needsTopPagination = chatList.atYBeginning;
        const nearTopPagination = isNearTopZone();
        const hasLatchedTopFromInteraction = pendingTopPaginationAfterInteraction;
        const userDrivenTopPagination = (needsTopPagination || nearTopPagination ||
                                         hasLatchedTopFromInteraction) &&
                                        userInteractedSinceBind &&
                                        !topPaginationRequestedForCurrentTop;
        const automaticUnderfillTopup = underfilled && !userInteractedSinceBind;

        if (isInteractionActive()) {
            if (userDrivenTopPagination && !underfilled)
                armPendingTopPaginationAfterInteraction();
            markPhase("qml.message_view.expand_if_needed.skip.interacting");
            return;
        }

        // After closing search we temporarily suppress top-driven expansion, but we
        // still must allow underfill recovery to avoid no-scroll dead-ends.
        if (suppressPaginationUntilUserScroll && !automaticUnderfillTopup)
            return;
        if (!automaticUnderfillTopup && !userDrivenTopPagination)
            return;

        markPhase("qml.message_view.expand_if_needed.trigger");

        if (automaticUnderfillTopup && !autoTopupAllowed) {
            markPhase("qml.message_view.expand_if_needed.skip.delayed");
            return;
        }

        if (automaticUnderfillTopup && autoExpandRemaining <= 0) {
            markPhase("qml.message_view.expand_if_needed.skip.budget");
            return;
        }

        if (!triggerRequestMore("qml.message_view.expand_if_needed.request"))
            return;
        if (automaticUnderfillTopup)
            autoExpandRemaining -= 1;
        if (userDrivenTopPagination) {
            // Top/near-top pagination should never re-pin the view to latest.
            chatList.keepPinnedToBottom = false;
            topPaginationRequestedForCurrentTop = true;
            pendingTopPaginationAfterInteraction = false;
        }
    }

    function maybeResetWindowAfterSearch() {
        const model = activeRoomModel;
        if (!pendingWindowResetAfterSearch || !model)
            return;
        if (model.paginationInProgress)
            return;

        // Collapse back to the configured initial slice after search mode ends.
        // This keeps scrollbar geometry and drag cost reasonable in normal mode.
        model.resetWindowToInitial();
        pendingWindowResetAfterSearch = false;
        topPaginationRequestedForCurrentTop = false;
        autoTopupAllowed = false;
        autoExpandRemaining = autoExpandBudgetPerBind;
        underfillTopupDelay.restart();
    }
}
