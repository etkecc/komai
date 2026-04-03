// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import cc.etke.komai

Item {
    id: support

    required property var rootItem
    required property var timelineList
    required property var topBar
    required property var dialogHost
    required property var messageActionSupport

    width: 0
    height: 0

    Timer {
        id: walkModeEntrySuppressTimer

        interval: 0
        onTriggered: support.rootItem.suppressNextWalkModeOlderStep = false
    }

    function focusedDelegate() {
        return rootItem.focusedEventId.length > 0
            ? (rootItem.visibleTimelineDelegates[rootItem.focusedEventId] || null)
            : null;
    }

    function primaryActionDelegate() {
        if (rootItem.primaryActionEventId.length === 0)
            return null;

        return rootItem.visibleTimelineDelegates[rootItem.primaryActionEventId] || null;
    }

    function primaryActionRoomModel() {
        const delegateItem = primaryActionDelegate();
        return delegateItem && delegateItem.roomModelOverride ? delegateItem.roomModelOverride : null;
    }

    function ensureFocusedDelegateVisible(eventId) {
        if (!timelineList)
            return;

        const row = matrixTimelineRowForEventId(eventId);
        if (row < 0)
            return;

        timelineList.forceLayout();

        const item = timelineList.itemAtIndex(row);
        if (!item || item.height <= 0)
            return;

        // Map delegate position into the ListView's coordinate space.
        // This works correctly regardless of BottomToTop layout direction.
        const mapped = timelineList.mapFromItem(item, 0, 0);
        const margin = 8;

        if (mapped.y < margin) {
            // Item is above the viewport — scroll up.
            timelineList.contentY += (mapped.y - margin);
        } else if (mapped.y + item.height > timelineList.height - margin) {
            // Item is below the viewport — scroll down.
            timelineList.contentY += (mapped.y + item.height - timelineList.height + margin);
        }
    }

    function focusWalkModeEventById(eventId, options) {
        let normalizedEventId = String(eventId || "");
        if (normalizedEventId.length === 0)
            return false;

        if (!rootItem.canExplicitlySelectEventId(normalizedEventId)) {
            const row = matrixTimelineRowForEventId(normalizedEventId);
            if (row < 0)
                return false;

            normalizedEventId = String(rootItem.selectableEventIdNearMatrixRow(row) || "");
            if (normalizedEventId.length === 0)
                return false;
        }

        rootItem.focusedEventId = normalizedEventId;
        if (!rootItem.walkModeActive && timelineList) {
            timelineList.keepPinnedToBottom = false;
            timelineList.userUnpinned = true;
        }
        rootItem.walkModeActive = true;
        const shouldDeferFocus = !!(options && options.deferFocus);
        if (shouldDeferFocus) {
            Qt.callLater(function () {
                if (rootItem.walkModeActive && rootItem.focusedEventId === normalizedEventId)
                    support.focusTimelineSelection();
            });
        } else {
            focusTimelineSelection();
        }

        const skipScroll = !!(options && options.skipScroll);
        if (!skipScroll)
            ensureFocusedDelegateVisible(normalizedEventId);

        return true;
    }

    function clearSelectedEvents() {
        if (rootItem.selectedEventIds.length === 0)
            return false;

        rootItem.selectedEventIds = [];
        rootItem.selectionAnchorEventId = "";
        return true;
    }

    function clearFocusedEvent() {
        rootItem.focusedEventId = "";
    }

    function clearWalkState(options) {
        const shouldFocusComposer = !!(options && options.focusComposer);

        clearSelectedEvents();
        clearFocusedEvent();
        rootItem.walkModeActive = false;
        rootItem.suppressNextWalkModeOlderStep = false;
        walkModeEntrySuppressTimer.stop();

        if (timelineList) {
            timelineList.keepPinnedToBottom = timelineList.atYEnd;
            if (timelineList.atYEnd)
                timelineList.userUnpinned = false;
        }

        if (shouldFocusComposer) {
            Qt.callLater(function () {
                rootItem.focusTextInput();
            });
        }
    }

    function handleMouseSelectionToggle(eventId) {
        const normalizedEventId = String(eventId || "");
        if (normalizedEventId.length === 0)
            return false;

        if (!rootItem.walkModeActive)
            clearWalkState({
                "focusComposer": false
            });

        if (!focusWalkModeEventById(normalizedEventId, {
                "skipScroll": true
            })) {
            return false;
        }

        const handled = rootItem.toggleSelectionForEventId(normalizedEventId);
        if (!handled)
            return false;

        if (rootItem.selectedEventIds.length === 0)
            rootItem.selectionAnchorEventId = "";

        return handled;
    }

    function enterWalkModeFromBottomMostVisible() {
        if (!rootItem.hasTimeline || rootItem.hasPendingAttachments || rootItem.editing)
            return false;
        if (TimelineManager.matrixTimelineReplyEventId.length > 0)
            return false;

        clearWalkState({
            "focusComposer": false
        });
        rootItem.suppressNextWalkModeOlderStep = true;
        walkModeEntrySuppressTimer.restart();

        const atLiveEdge = rootItem.isEffectivelyAtLiveEdge();
        const bottomMostEventId = rootItem.bottomMostVisibleEventId();

        if (atLiveEdge) {
            return focusLatestWalkModeEvent({
                "deferFocus": true
            });
        }

        if (bottomMostEventId.length === 0) {
            return focusLatestWalkModeEvent({
                "deferFocus": true
            });
        }

        return focusWalkModeEventById(targetEventId, {
            "skipScroll": true,
            "deferFocus": true
        });
    }

    function enterWalkModeAndMoveTowardOlderEventsByChunk() {
        if (!rootItem.walkModeActive) {
            if (!enterWalkModeFromBottomMostVisible())
                return false;
        }

        return moveFocusTowardOlderEventsByChunk() || rootItem.walkModeActive;
    }

    function lastRoomHeaderActionButtonTarget() {
        return topBar && typeof topBar.lastVisibleActionButtonItem === "function"
            ? topBar.lastVisibleActionButtonItem()
            : null;
    }

    function handleEscape() {
        if (!rootItem.walkModeActive && rootItem.selectedEventIds.length === 0 && !rootItem.hasFocusedEvent)
            return false;

        if (rootItem.selectedEventIds.length > 0) {
            clearSelectedEvents();
            focusTimelineSelection();
            return true;
        }

        return exitWalkMode({
            "focusComposer": true
        });
    }

    function matrixTimelineRowForEventId(eventId) {
        const normalizedEventId = String(eventId || "");
        if (normalizedEventId.length === 0 || !TimelineManager.matrixTimelineModel)
            return -1;

        return TimelineManager.matrixTimelineModel.rowForEventId(normalizedEventId);
    }

    function isSelectableMatrixTimelineRow(row) {
        if (!TimelineManager.matrixTimelineModel || row < 0 || row >= TimelineManager.matrixTimelineItemCount)
            return false;

        const item = TimelineManager.matrixTimelineModel.itemAt(row);
        return !!item
            && String(item.eventId || "").length > 0
            && String(item.typeString || "") !== "date_divider"
            && !Boolean(item.isHiddenEvent);
    }

    function focusMatrixTimelineRow(row, options) {
        if (!isSelectableMatrixTimelineRow(row))
            return false;

        const item = TimelineManager.matrixTimelineModel.itemAt(row);
        return focusWalkModeEventById(String(item.eventId || ""), options || {});
    }

    function moveFocusByStep(step) {
        const currentRow = matrixTimelineRowForEventId(rootItem.focusedEventId);
        if (currentRow < 0)
            return false;

        for (let row = currentRow + step; row >= 0 && row < TimelineManager.matrixTimelineItemCount; row += step) {
            if (focusMatrixTimelineRow(row))
                return true;
        }

        return false;
    }

    function walkModeChunkSize() {
        if (!timelineList)
            return 4;

        return Math.max(4, Math.floor(Math.max(timelineList.height, 1) / 240));
    }

    function moveFocusByChunk(step) {
        const currentRow = matrixTimelineRowForEventId(rootItem.focusedEventId);
        if (currentRow < 0)
            return false;

        let remaining = walkModeChunkSize();
        for (let row = currentRow + step; row >= 0 && row < TimelineManager.matrixTimelineItemCount; row += step) {
            if (!isSelectableMatrixTimelineRow(row))
                continue;

            remaining -= 1;
            if (remaining <= 0)
                return focusMatrixTimelineRow(row);
        }

        return false;
    }

    function moveFocusTowardOlderEvents() {
        return moveFocusByStep(1);
    }

    function moveFocusTowardNewerEvents() {
        return moveFocusByStep(-1);
    }

    function moveFocusTowardOlderEventsByChunk() {
        return moveFocusByChunk(1);
    }

    function moveFocusTowardNewerEventsByChunk() {
        return moveFocusByChunk(-1);
    }

    function focusOldestLoadedWalkModeEvent(options) {
        for (let row = TimelineManager.matrixTimelineItemCount - 1; row >= 0; row--) {
            if (focusMatrixTimelineRow(row, options || {}))
                return true;
        }

        return false;
    }

    function focusLatestWalkModeEvent(options) {
        for (let row = 0; row < TimelineManager.matrixTimelineItemCount; row++) {
            if (focusMatrixTimelineRow(row, options || {}))
                return true;
        }

        return false;
    }

    function eventUsesWalkModeModifiers(event) {
        const modifiers = Number(event.modifiers);
        return (modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier)) === 0;
    }

    function eventUsesCtrlWalkModeModifiers(event) {
        const modifiers = Number(event.modifiers);
        return (modifiers & Qt.ControlModifier) !== 0
            && (modifiers & (Qt.AltModifier | Qt.MetaModifier | Qt.ShiftModifier)) === 0;
    }

    function eventMatchesWalkModeLatinKey(event, latinKey) {
        if (!event)
            return false;

        return LayoutAgnosticKeys.matchesLatinKey(latinKey,
                                                  event.key,
                                                  event.nativeScanCode);
    }

    function isWalkModeEnterKey(event) {
        return event.key === Qt.Key_Return || event.key === Qt.Key_Enter;
    }

    function isWalkModeOptionsKey(event) {
        return (event.key === Qt.Key_Menu && eventUsesWalkModeModifiers(event))
            || (eventMatchesWalkModeLatinKey(event, LayoutAgnosticKeys.LatinKey.O)
                && eventUsesWalkModeModifiers(event));
    }

    function isWalkModeHelpKey(event) {
        if (!event)
            return false;

        const text = String(event.text || "");
        const modifiers = Number(event.modifiers);
        return text === "?" && (modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier)) === 0;
    }

    function openPrimaryMessageActionsDialog() {
        const delegateItem = primaryActionDelegate();
        const roomModel = primaryActionRoomModel();
        if (!delegateItem || !roomModel)
            return false;

        return messageActionSupport.openOptionsDialog(rootItem, delegateItem, roomModel);
    }

    function canPerformWalkModeAction(actionName) {
        const delegateItem = primaryActionDelegate();
        const roomModel = primaryActionRoomModel();
        if (!delegateItem || !roomModel)
            return false;

        switch (actionName) {
        case "reply":
            return messageActionSupport.canReply(delegateItem, roomModel);
        case "thread":
            return messageActionSupport.canThread(delegateItem, roomModel);
        case "edit":
            return messageActionSupport.canEdit(delegateItem, roomModel);
        case "forward":
            return messageActionSupport.canForward(delegateItem);
        case "remove":
            return messageActionSupport.canRemove(delegateItem, roomModel);
        case "options":
            return messageActionSupport.canOpenOptions(delegateItem);
        case "raw":
            return messageActionSupport.canViewRaw(delegateItem);
        default:
            return false;
        }
    }

    function performWalkModeAction(actionName) {
        const delegateItem = primaryActionDelegate();
        const roomModel = primaryActionRoomModel();
        if (!delegateItem || !roomModel)
            return false;

        const exitsToComposer = actionName === "reply" || actionName === "thread" || actionName === "edit";
        if (exitsToComposer) {
            exitWalkMode({
                "focusComposer": false
            });
        }

        switch (actionName) {
        case "reply":
            return messageActionSupport.applyReply(roomModel, delegateItem);
        case "thread":
            return messageActionSupport.applyThread(roomModel, delegateItem);
        case "edit":
            return messageActionSupport.applyEdit(roomModel, delegateItem);
        case "forward":
            return messageActionSupport.applyForward(rootItem, roomModel, delegateItem);
        case "remove":
            return messageActionSupport.applyRemove(rootItem, roomModel, delegateItem);
        case "raw":
            return messageActionSupport.applyViewRaw(roomModel, delegateItem);
        case "options":
            return messageActionSupport.openOptionsDialog(rootItem, delegateItem, roomModel);
        default:
            return false;
        }
    }

    function handleWalkModeKey(event) {
        if (!event || !rootItem.walkModeActive)
            return false;

        if (event.key === Qt.Key_Escape) {
            handleEscape();
            event.accepted = true;
            return true;
        }

        if (event.key === Qt.Key_Up
                && (event.modifiers === Qt.NoModifier || event.modifiers === Qt.KeypadModifier)) {
            if (rootItem.suppressNextWalkModeOlderStep) {
                rootItem.suppressNextWalkModeOlderStep = false;
                event.accepted = true;
                return true;
            }

            moveFocusTowardOlderEvents();
            event.accepted = true;
            return true;
        }

        if (event.key === Qt.Key_Down
                && (event.modifiers === Qt.NoModifier || event.modifiers === Qt.KeypadModifier)) {
            moveFocusTowardNewerEvents();
            event.accepted = true;
            return true;
        }

        if (event.key === Qt.Key_Space && event.modifiers === Qt.NoModifier) {
            if (rootItem.focusedEventId.length > 0)
                rootItem.toggleSelectionForEventId(rootItem.focusedEventId);
            event.accepted = true;
            return true;
        }

        if (eventMatchesWalkModeLatinKey(event, LayoutAgnosticKeys.LatinKey.R) && eventUsesWalkModeModifiers(event)) {
            performWalkModeAction("reply");
            event.accepted = true;
            return true;
        }

        if (eventMatchesWalkModeLatinKey(event, LayoutAgnosticKeys.LatinKey.T) && eventUsesWalkModeModifiers(event)) {
            performWalkModeAction("thread");
            event.accepted = true;
            return true;
        }

        if (eventMatchesWalkModeLatinKey(event, LayoutAgnosticKeys.LatinKey.E) && eventUsesWalkModeModifiers(event)) {
            performWalkModeAction("edit");
            event.accepted = true;
            return true;
        }

        if (eventMatchesWalkModeLatinKey(event, LayoutAgnosticKeys.LatinKey.F) && eventUsesWalkModeModifiers(event)) {
            performWalkModeAction("forward");
            event.accepted = true;
            return true;
        }

        if (eventMatchesWalkModeLatinKey(event, LayoutAgnosticKeys.LatinKey.D) && eventUsesWalkModeModifiers(event)) {
            performWalkModeAction("remove");
            event.accepted = true;
            return true;
        }

        if (eventMatchesWalkModeLatinKey(event, LayoutAgnosticKeys.LatinKey.U) && eventUsesWalkModeModifiers(event)) {
            performWalkModeAction("raw");
            event.accepted = true;
            return true;
        }

        if (event.modifiers === Qt.ControlModifier
                && LayoutAgnosticKeys.matchesLatinKey(LayoutAgnosticKeys.LatinKey.U,
                                                      event.key,
                                                      event.nativeScanCode)) {
            moveFocusTowardOlderEventsByChunk();
            event.accepted = true;
            return true;
        }

        if (event.modifiers === Qt.ControlModifier
                && LayoutAgnosticKeys.matchesLatinKey(LayoutAgnosticKeys.LatinKey.D,
                                                      event.key,
                                                      event.nativeScanCode)) {
            moveFocusTowardNewerEventsByChunk();
            event.accepted = true;
            return true;
        }

        if (isWalkModeHelpKey(event)) {
            openWalkModeHelpDialog();
            event.accepted = true;
            return true;
        }

        if (isWalkModeOptionsKey(event)) {
            openPrimaryMessageActionsDialog();
            event.accepted = true;
            return true;
        }

        if (isWalkModeEnterKey(event) && event.modifiers === Qt.NoModifier) {
            openPrimaryMessageActionsDialog();
            event.accepted = true;
            return true;
        }

        return false;
    }

    function exitWalkMode(options) {
        if (!rootItem.walkModeActive && !rootItem.hasFocusedEvent && !rootItem.hasSelectedEvents)
            return false;

        clearWalkState(options);
        return true;
    }

    function timelineSelectionFocusTarget() {
        return timelineList;
    }

    function focusTimelineSelection() {
        if (!timelineList)
            return false;

        timelineList.forceActiveFocus();
        return true;
    }

    function openWalkModeHelpDialog() {
        const component = Qt.createComponent("qrc:/resources/qml/dialogs/timeline/SelectionModeHelpDialog.qml");
        if (component.status !== Component.Ready) {
            console.error("SelectionModeHelpDialog: " + component.errorString());
            return false;
        }

        const dialogParent = dialogHost ? dialogHost : rootItem;
        const dialog = component.createObject(dialogParent, {
            "appRoot": dialogParent
        });
        if (!dialog)
            return false;

        dialog.open();
        rootItem.destroyOnClose(dialog);
        return true;
    }
}
