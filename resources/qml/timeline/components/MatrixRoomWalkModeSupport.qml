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

    property bool _pendingGoToTop: false

    Timer {
        id: walkModeGoToTopSequenceTimer

        interval: 400
        repeat: false
        onTriggered: support._pendingGoToTop = false
    }

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

    // Returns a message-model-like object for the primary action event.
    // Prefers the registered visual delegate; falls back to model data
    // when the delegate hasn't been created yet (async scroll/recycle).
    function primaryActionMessageModel() {
        const delegate = primaryActionDelegate();
        if (delegate)
            return delegate;

        const eventId = rootItem.primaryActionEventId;
        if (eventId.length === 0)
            return null;

        const model = rootItem.perRoomModel;
        if (!model)
            return null;

        const row = model.rowForEventId(eventId);
        if (row < 0)
            return null;

        const item = model.itemAt(row);
        if (!item)
            return null;

        return {
            "eventId": String(item.eventId || ""),
            "type": Number(item.type || 0),
            "isStateEvent": Boolean(item.isStateEvent),
            "isEditable": Boolean(item.isEditable),
            "isSender": Boolean(item.isSender),
            "isEncrypted": Boolean(item.isEncrypted),
            "threadId": String(item.threadId || ""),
            "body": String(item.body || ""),
            "formattedBody": String(item.formattedBody || "")
        };
    }

    function primaryActionRoomModel() {
        const delegateItem = primaryActionDelegate();
        if (delegateItem && delegateItem.roomModelOverride)
            return delegateItem.roomModelOverride;

        return rootItem.messageActionsRoomModel || null;
    }

    function ensureFocusedDelegateVisible(eventId) {
        if (!timelineList)
            return;

        const row = matrixTimelineRowForEventId(eventId);
        if (row < 0)
            return;

        timelineList.forceLayout();
        timelineList.positionViewAtIndex(row, ListView.Contain);
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
        resetWalkModeGoToTopSequence();

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

    function handleMouseSelectionRangeTo(eventId) {
        const normalizedEventId = String(eventId || "");
        if (normalizedEventId.length === 0)
            return false;

        if (!rootItem.walkModeActive || String(rootItem.selectionAnchorEventId || "").length === 0)
            return handleMouseSelectionToggle(normalizedEventId);

        if (!focusWalkModeEventById(normalizedEventId, {
                "skipScroll": true
            })) {
            return false;
        }

        return rootItem.selectRangeToEventId(normalizedEventId);
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
        if (normalizedEventId.length === 0 || !rootItem.perRoomModel)
            return -1;

        return rootItem.perRoomModel.rowForEventId(normalizedEventId);
    }

    function isSelectableMatrixTimelineRow(row) {
        if (!rootItem.perRoomModel || row < 0 || row >= (rootItem.perRoomModel ? rootItem.perRoomModel.count : 0))
            return false;

        const item = rootItem.perRoomModel.itemAt(row);
        if (!item
            || String(item.eventId || "").length === 0
            || String(item.typeString || "") === "date_divider"
            || Boolean(item.isHiddenEvent))
            return false;

        // Skip collapsed thread replies (not visible in the timeline).
        if (rootItem.filteredTimeline.collapseThreadReplies
                && String(item.threadId || "").length > 0
                && !Boolean(item.isThreadRoot))
            return false;

        return true;
    }

    function focusMatrixTimelineRow(row, options) {
        if (!isSelectableMatrixTimelineRow(row))
            return false;

        const item = rootItem.perRoomModel.itemAt(row);
        return focusWalkModeEventById(String(item.eventId || ""), options || {});
    }

    function moveFocusByStep(step) {
        const currentRow = matrixTimelineRowForEventId(rootItem.focusedEventId);
        if (currentRow < 0)
            return false;

        for (let row = currentRow + step; row >= 0 && row < (rootItem.perRoomModel ? rootItem.perRoomModel.count : 0); row += step) {
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
        for (let row = currentRow + step; row >= 0 && row < (rootItem.perRoomModel ? rootItem.perRoomModel.count : 0); row += step) {
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
        for (let row = (rootItem.perRoomModel ? rootItem.perRoomModel.count : 0) - 1; row >= 0; row--) {
            if (focusMatrixTimelineRow(row, options || {}))
                return true;
        }

        return false;
    }

    function focusLatestWalkModeEvent(options) {
        for (let row = 0; row < (rootItem.perRoomModel ? rootItem.perRoomModel.count : 0); row++) {
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

    function eventUsesNoWalkModeModifiers(event) {
        const modifiers = Number(event.modifiers);
        return (modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier | Qt.ShiftModifier)) === 0;
    }

    function eventUsesShiftOnlyWalkModeModifiers(event) {
        const modifiers = Number(event.modifiers);
        return (modifiers & Qt.ShiftModifier) !== 0
            && (modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier)) === 0;
    }

    function resetWalkModeGoToTopSequence() {
        _pendingGoToTop = false;
        walkModeGoToTopSequenceTimer.stop();
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

    function keyboardActionsControl() {
        const delegate = primaryActionDelegate();
        if (!delegate)
            return null;
        const actions = delegate.messageActions;
        return (actions && actions.keyboardActive) ? actions : null;
    }

    function openPrimaryMessageActionsDialog() {
        const msgModel = primaryActionMessageModel();
        const roomModel = primaryActionRoomModel();
        if (!msgModel || !roomModel)
            return false;

        return messageActionSupport.openOptionsDialog(rootItem, msgModel, roomModel);
    }

    function selectedEventIdsForAction(actionName) {
        const model = rootItem.perRoomModel;
        if (!model)
            return [];

        const roomModel = primaryActionRoomModel();
        const selected = rootItem.selectedEventIds;
        const entries = [];
        for (let i = 0; i < selected.length; i++) {
            const eid = String(selected[i] || "");
            if (eid.length === 0)
                continue;

            const row = model.rowForEventId(eid);
            if (row < 0)
                continue;

            const item = model.itemAt(row);
            if (!item)
                continue;

            const type = Number(item.type || 0);
            if (actionName === "forward" && messageActionSupport.isForwardableType(type))
                entries.push({ "row": row, "eid": eid });
            else if (actionName === "remove" && roomModel
                     && type !== MtxEvent.Redacted
                     && (Boolean(item.isSender)
                         || (roomModel.permissions && roomModel.permissions.canRedact())))
                entries.push({ "row": row, "eid": eid });
        }

        // Sort by timeline position (row 0 = newest in the reversed
        // model, so descending row order gives chronological order).
        entries.sort((a, b) => b.row - a.row);
        return entries.map(e => e.eid);
    }

    function orderedExistingEventIds(eventIds) {
        const model = rootItem.perRoomModel;
        if (!model)
            return [];

        const entries = [];
        for (let i = 0; i < eventIds.length; i++) {
            const eid = String(eventIds[i] || "");
            if (eid.length === 0)
                continue;

            const row = model.rowForEventId(eid);
            if (row < 0)
                continue;

            entries.push({ "row": row, "eid": eid });
        }

        entries.sort((a, b) => b.row - a.row);
        return entries.map(e => e.eid);
    }

    function copySelectionModeText(plainText) {
        const roomModel = rootItem.messageActionsRoomModel;
        if (!roomModel || typeof roomModel.copyTextForEventIds !== "function")
            return false;

        const eventIds = rootItem.selectedEventIds.length > 0
            ? orderedExistingEventIds(rootItem.selectedEventIds)
            : (rootItem.focusedEventId.length > 0 ? [rootItem.focusedEventId] : []);
        if (eventIds.length === 0)
            return false;

        const copiedText = String(roomModel.copyTextForEventIds(eventIds, !!plainText) || "");
        if (copiedText.length === 0)
            return false;

        Clipboard.text = copiedText;
        return true;
    }

    function canPerformWalkModeAction(actionName) {
        // Multi-select actions: check selected events directly.
        if (rootItem.selectedCount > 1 && (actionName === "forward" || actionName === "remove"))
            return selectedEventIdsForAction(actionName).length > 0;

        const msgModel = primaryActionMessageModel();
        const roomModel = primaryActionRoomModel();
        if (!msgModel || !roomModel)
            return false;

        switch (actionName) {
        case "reply":
            return messageActionSupport.canReply(msgModel, roomModel);
        case "thread":
            return messageActionSupport.canThread(msgModel, roomModel);
        case "edit":
            return messageActionSupport.canEdit(msgModel, roomModel);
        case "forward":
            return messageActionSupport.canForward(msgModel);
        case "remove":
            return messageActionSupport.canRemove(msgModel, roomModel);
        case "options":
            return messageActionSupport.canOpenOptions(msgModel);
        case "raw":
            return messageActionSupport.canViewRaw(msgModel);
        default:
            return false;
        }
    }

    function performWalkModeAction(actionName) {
        // Multi-select actions: forward and remove operate on all eligible selected IDs.
        if (rootItem.selectedCount > 1 && (actionName === "forward" || actionName === "remove")) {
            const eligibleIds = selectedEventIdsForAction(actionName);
            if (eligibleIds.length === 0)
                return false;

            if (actionName === "forward")
                return rootItem.openForwardDialogForEvents(eligibleIds, rootItem.selectedCount);

            return rootItem.openRemoveMessagesDialog(eligibleIds, rootItem.selectedCount);
        }

        const msgModel = primaryActionMessageModel();
        const roomModel = primaryActionRoomModel();
        if (!msgModel || !roomModel)
            return false;

        const exitsToComposer = actionName === "reply" || actionName === "thread" || actionName === "edit";
        if (exitsToComposer) {
            exitWalkMode({
                "focusComposer": false
            });
        }

        switch (actionName) {
        case "reply":
            return messageActionSupport.applyReply(roomModel, msgModel);
        case "thread":
            return messageActionSupport.applyThread(roomModel, msgModel);
        case "edit":
            return messageActionSupport.applyEdit(roomModel, msgModel);
        case "forward":
            return messageActionSupport.applyForward(rootItem, roomModel, msgModel);
        case "remove":
            return messageActionSupport.applyRemove(rootItem, roomModel, msgModel);
        case "raw":
            return messageActionSupport.applyViewRaw(roomModel, msgModel);
        case "options":
            return messageActionSupport.openOptionsDialog(rootItem, msgModel, roomModel);
        default:
            return false;
        }
    }

    function handleWalkModeKey(event) {
        if (!event || !rootItem.walkModeActive)
            return false;

        const gKeyPressed = eventMatchesWalkModeLatinKey(event, LayoutAgnosticKeys.LatinKey.G);
        const plainGPressed = gKeyPressed && eventUsesNoWalkModeModifiers(event);
        const shiftGPressed = gKeyPressed && eventUsesShiftOnlyWalkModeModifiers(event);

        const kbActions = keyboardActionsControl();
        if (kbActions) {
            if (event.key === Qt.Key_Escape) {
                kbActions.dismiss();
                event.accepted = true;
                return true;
            }

            if ((event.key === Qt.Key_Left
                        || eventMatchesWalkModeLatinKey(event, LayoutAgnosticKeys.LatinKey.H))
                    && eventUsesWalkModeModifiers(event)) {
                kbActions.moveFocus(-1);
                event.accepted = true;
                return true;
            }

            if ((event.key === Qt.Key_Right
                        || eventMatchesWalkModeLatinKey(event, LayoutAgnosticKeys.LatinKey.L))
                    && eventUsesWalkModeModifiers(event)) {
                kbActions.moveFocus(1);
                event.accepted = true;
                return true;
            }

            if (kbActions.usesTwoRowLayout()) {
                if ((event.key === Qt.Key_Up
                            || eventMatchesWalkModeLatinKey(event, LayoutAgnosticKeys.LatinKey.K))
                        && eventUsesWalkModeModifiers(event)) {
                    kbActions.moveFocus(-1);
                    event.accepted = true;
                    return true;
                }

                if ((event.key === Qt.Key_Down
                            || eventMatchesWalkModeLatinKey(event, LayoutAgnosticKeys.LatinKey.J))
                        && eventUsesWalkModeModifiers(event)) {
                    kbActions.moveFocus(1);
                    event.accepted = true;
                    return true;
                }
            }

            if (isWalkModeEnterKey(event) && event.modifiers === Qt.NoModifier) {
                kbActions.activateFocusedButton();
                event.accepted = true;
                return true;
            }

            if (event.key === Qt.Key_Space && event.modifiers === Qt.NoModifier) {
                kbActions.activateFocusedButton();
                event.accepted = true;
                return true;
            }

            if (shiftGPressed) {
                kbActions.focusLastVisibleButton();
                event.accepted = true;
                return true;
            }

            if (plainGPressed) {
                if (_pendingGoToTop) {
                    resetWalkModeGoToTopSequence();
                    kbActions.focusFirstVisibleButton();
                } else {
                    _pendingGoToTop = true;
                    walkModeGoToTopSequenceTimer.restart();
                }
                event.accepted = true;
                return true;
            }
        }

        if (event.key === Qt.Key_Escape) {
            handleEscape();
            event.accepted = true;
            return true;
        }

        if (!plainGPressed)
            resetWalkModeGoToTopSequence();

        if ((event.key === Qt.Key_Up && (event.modifiers === Qt.NoModifier || event.modifiers === Qt.KeypadModifier))
                || (eventMatchesWalkModeLatinKey(event, LayoutAgnosticKeys.LatinKey.K) && eventUsesWalkModeModifiers(event))) {
            if (rootItem.suppressNextWalkModeOlderStep) {
                rootItem.suppressNextWalkModeOlderStep = false;
                event.accepted = true;
                return true;
            }

            moveFocusTowardOlderEvents();
            event.accepted = true;
            return true;
        }

        if ((event.key === Qt.Key_Down && (event.modifiers === Qt.NoModifier || event.modifiers === Qt.KeypadModifier))
                || (eventMatchesWalkModeLatinKey(event, LayoutAgnosticKeys.LatinKey.J) && eventUsesWalkModeModifiers(event))) {
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

        if (shiftGPressed) {
            focusLatestWalkModeEvent();
            event.accepted = true;
            return true;
        }

        if (plainGPressed) {
            if (_pendingGoToTop) {
                resetWalkModeGoToTopSequence();
                focusOldestLoadedWalkModeEvent();
            } else {
                _pendingGoToTop = true;
                walkModeGoToTopSequenceTimer.restart();
            }
            event.accepted = true;
            return true;
        }

        if (isWalkModeHelpKey(event)) {
            openWalkModeHelpDialog();
            event.accepted = true;
            return true;
        }

        if (eventMatchesWalkModeLatinKey(event, LayoutAgnosticKeys.LatinKey.I) && eventUsesWalkModeModifiers(event)) {
            exitWalkMode({
                "focusComposer": true
            });
            event.accepted = true;
            return true;
        }

        if (isWalkModeOptionsKey(event)) {
            openPrimaryMessageActionsDialog();
            event.accepted = true;
            return true;
        }

        if (isWalkModeEnterKey(event) && event.modifiers === Qt.NoModifier) {
            const delegate = primaryActionDelegate();
            if (delegate && typeof delegate.openKeyboardMessageActions === "function")
                delegate.openKeyboardMessageActions();
            else
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
