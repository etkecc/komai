// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15
import QtQuick.Controls 2.15
import cc.etke.komai 1.0

Item {
    id: chatRoot

    required property var emojiPopup
    required property var dialogHost
    required property var componentCatalog
    property int availableWidth: width
    property int padding: Komai.paddingMedium
    property bool composerAvailable: true
    property var selectionModeBar: null
    property var roomHeader: null
    property string searchString: ""
    property bool filterByNotifications: false
    property bool disableTimelineList: false
    property bool roomSearchHasFocus: false
    property bool suppressRoomSwitchSpinner: false
    readonly property bool filteringInProgress: filteredTimeline.filteringInProgress
    readonly property bool filteringRequested: searchString.length > 0 || filterByNotifications || (activeRoomModel && activeRoomModel.thread !== "")
    property bool perfFirstVisibleItemLogged: false
    property Room roommodel: room
    property var activeRoomModel: null
    property var pendingRoomModel: null
    property bool roomSwitchInProgress: false
    property int roomSwitchBindSerial: 0
    property bool walkModeActive: false
    property string focusedEventId: ""
    property var selectedEventIds: []
    property string selectionAnchorEventId: ""
    property bool keyboardActionsOpen: false
    property var visibleTimelineDelegates: ({})
    property string pendingKeyboardActionsEventId: ""
    property bool pendingWalkModeGoToTopRequest: false
    readonly property bool hasFocusedEvent: focusedEventId.length > 0
    readonly property bool hasSelectedEvents: selectedEventIds.length > 0
    readonly property int selectedCount: selectedEventIds.length
    readonly property bool hasSingleSelection: selectedCount === 1
    readonly property string singleSelectedEventId: hasSingleSelection ? String(selectedEventIds[0]) : ""
    readonly property string primaryActionEventId: hasSingleSelection
        ? singleSelectedEventId
        : (!hasSelectedEvents ? focusedEventId : "")
    readonly property real listViewDisplayMargin: roomSwitchInProgress ? 0 : chat.height / 8
    readonly property real listViewCacheBuffer: roomSwitchInProgress ? 0 : 320
    readonly property int walkModeOlderPrefetchThresholdItems: 6
    readonly property int walkModeChunkDivisor: 2
    readonly property bool selectionModeEnterShortcutEnabled: {
        if (!(walkModeActive || keyboardActionsOpen))
            return false;

        const activeItem = chatRoot.Window.activeFocusItem;
        return itemIsInSubtree(activeItem, chatRoot)
            && !(selectionModeBar && itemIsInSubtree(activeItem, selectionModeBar));
    }

    MessageActionSupport {
        id: messageActionSupport
    }

    function resetVisibleDelegateRegistry() {
        visibleTimelineDelegates = ({});
    }

    function registerVisibleDelegate(eventId, delegate) {
        if (!eventId || !delegate)
            return;

        visibleTimelineDelegates[eventId] = delegate;
        if (pendingKeyboardActionsEventId === eventId)
            Qt.callLater(tryOpenPendingKeyboardActions);
    }

    function unregisterVisibleDelegate(eventId, delegate) {
        if (!eventId)
            return;

        if (!delegate || visibleTimelineDelegates[eventId] === delegate)
            delete visibleTimelineDelegates[eventId];
    }

    function keyboardActionsControl() {
        if (typeof messageActionsHost === "undefined" || !messageActionsHost || !messageActionsHost.control)
            return null;

        return messageActionsHost.control;
    }

    function sameEventIdList(left, right) {
        if (left.length !== right.length)
            return false;

        for (let index = 0; index < left.length; index++) {
            if (String(left[index]) !== String(right[index]))
                return false;
        }

        return true;
    }

    function selectionModeBarHasFocus() {
        if (!selectionModeBar)
            return false;

        return itemIsInSubtree(chatRoot.Window.activeFocusItem, selectionModeBar);
    }

    function focusFirstSelectionModeBarButton() {
        if (!selectionModeBar || typeof selectionModeBar.focusFirstVisibleButton !== "function")
            return false;

        return selectionModeBar.focusFirstVisibleButton();
    }

    function focusLastSelectionModeBarButton() {
        if (!selectionModeBar || typeof selectionModeBar.focusLastVisibleButton !== "function")
            return false;

        return selectionModeBar.focusLastVisibleButton();
    }

    function moveSelectionModeBarFocus(step) {
        if (!selectionModeBar || typeof selectionModeBar.moveFocus !== "function")
            return false;

        return selectionModeBar.moveFocus(step);
    }

    function focusLastRoomHeaderActionButton() {
        if (!roomHeader || typeof roomHeader.focusLastVisibleActionButton !== "function")
            return false;

        return roomHeader.focusLastVisibleActionButton();
    }

    function timelineSelectionFocusTarget() {
        return chat;
    }

    function firstSelectionModeBarTabTarget() {
        if (!selectionModeBar || typeof selectionModeBar.firstVisibleButtonItem !== "function")
            return null;

        return selectionModeBar.firstVisibleButtonItem();
    }

    function lastRoomHeaderActionButtonTarget() {
        if (!roomHeader || typeof roomHeader.lastVisibleActionButtonItem !== "function")
            return null;

        return roomHeader.lastVisibleActionButtonItem();
    }

    function normalizedEventIds(eventIds) {
        const normalizedIds = [];
        const seenIds = ({});

        for (let index = 0; index < eventIds.length; index++) {
            const eventId = String(eventIds[index] || "");
            if (!eventId || seenIds[eventId])
                continue;

            seenIds[eventId] = true;
            normalizedIds.push(eventId);
        }

        return normalizedIds;
    }

    function clearSelectedEvents() {
        if (selectedEventIds.length === 0)
            return false;

        selectedEventIds = [];
        selectionAnchorEventId = "";
        return true;
    }

    function clearFocusedEvent() {
        focusedEventId = "";
    }

    function focusTimelineSelection() {
        if (typeof chat === "undefined" || !chat)
            return false;

        chat.forceActiveFocus(Qt.ShortcutFocusReason);
        return true;
    }

    function displayedEventIdAt(index) {
        if (index < 0 || index >= chat.count || !chat.model || typeof chat.model.dataByIndex !== "function")
            return "";

        const value = chat.model.dataByIndex(index, Room.EventId);
        return value === undefined || value === null ? "" : String(value);
    }

    function displayedEventHiddenAt(index) {
        if (index < 0 || index >= chat.count || !chat.model || typeof chat.model.dataByIndex !== "function")
            return true;

        return !!chat.model.dataByIndex(index, Room.IsHiddenEvent);
    }

    function displayedIndexForEventId(eventId) {
        if (!eventId || !chat.model)
            return -1;

        for (let index = 0; index < chat.count; index++) {
            if (displayedEventIdAt(index) === eventId)
                return index;
        }

        return -1;
    }

    function itemIsInSubtree(item, ancestor) {
        let current = item;
        while (current) {
            if (current === ancestor)
                return true;
            current = current.parent;
        }

        return false;
    }

    function selectedEventIdsContains(eventId) {
        if (!eventId)
            return false;

        return selectedEventIds.indexOf(String(eventId)) >= 0;
    }

    function canExplicitlySelectEventId(eventId) {
        return messageInfoForEventId(eventId) !== null;
    }

    function updateSelectionAnchor(preferredEventId) {
        const normalizedPreferredEventId = String(preferredEventId || "");
        if (normalizedPreferredEventId && selectedEventIdsContains(normalizedPreferredEventId)) {
            selectionAnchorEventId = normalizedPreferredEventId;
            return;
        }

        selectionAnchorEventId = selectedEventIds.length > 0
            ? String(selectedEventIds[selectedEventIds.length - 1] || "")
            : "";
    }

    function toggleSelectionForEventId(eventId) {
        const normalizedEventId = String(eventId || "");
        if (!normalizedEventId || !canExplicitlySelectEventId(normalizedEventId))
            return false;

        const wasSelected = selectedEventIdsContains(normalizedEventId);
        if (wasSelected) {
            selectedEventIds = selectedEventIds.filter(function (selectedEventId) {
                return selectedEventId !== normalizedEventId;
            });
        } else {
            selectedEventIds = normalizedEventIds(selectedEventIds.concat([normalizedEventId]));
        }

        updateSelectionAnchor(wasSelected ? "" : normalizedEventId);
        return true;
    }

    function focusedDelegate() {
        if (!focusedEventId)
            return null;

        return visibleTimelineDelegates[focusedEventId] || null;
    }

    function visibleWalkModeDelegatesInViewport() {
        const delegates = [];
        const viewportTop = chat.contentY;
        const viewportBottom = viewportTop + chat.height;

        for (const eventId in visibleTimelineDelegates) {
            const delegate = visibleTimelineDelegates[eventId];
            if (!delegate || delegate.visible === false || delegate.height <= 0)
                continue;

            const delegateTop = delegate.y;
            const delegateBottom = delegate.y + delegate.height;
            if (delegateBottom <= viewportTop || delegateTop >= viewportBottom)
                continue;

            delegates.push(delegate);
        }

        delegates.sort(function (left, right) {
            return left.y - right.y;
        });
        return delegates;
    }

    function bottomMostVisibleDelegate() {
        const delegates = visibleWalkModeDelegatesInViewport();
        return delegates.length > 0 ? delegates[delegates.length - 1] : null;
    }

    function scrollDisplayedIndexIntoView(index) {
        if (index < 0)
            return;

        chat.keepPinnedToBottom = false;
        chat.positionViewAtIndex(index, ListView.Visible);
        chat.updateLastScroll();
    }

    function resetWalkModeGoToTopSequence() {
        pendingWalkModeGoToTopRequest = false;
        walkModeGoToTopSequenceTimer.stop();
    }

    function focusDisplayedIndex(index, options) {
        if (index < 0 || index >= chat.count)
            return false;
        if (displayedEventHiddenAt(index))
            return false;

        const eventId = displayedEventIdAt(index);
        if (!eventId)
            return false;

        pendingKeyboardActionsEventId = "";
        focusedEventId = eventId;
        focusTimelineSelection();
        if (!(options && options.skipScroll))
            scrollDisplayedIndexIntoView(index);
        if (options && options.prefetchOlder)
            maybePrefetchOlderTimelineForWalk(index);
        return true;
    }

    function focusWalkModeEventById(eventId, options) {
        const normalizedEventId = String(eventId || "");
        const index = displayedIndexForEventId(normalizedEventId);
        if (index < 0 || displayedEventHiddenAt(index))
            return false;

        closeKeyboardActions({
            "skipTimelineFocus": true
        });
        pendingKeyboardActionsEventId = "";
        resetWalkModeGoToTopSequence();
        focusedEventId = normalizedEventId;
        walkModeActive = true;
        focusTimelineSelection();
        if (!(options && options.skipScroll))
            scrollDisplayedIndexIntoView(index);
        if (options && options.prefetchOlder)
            maybePrefetchOlderTimelineForWalk(index);
        return true;
    }

    function findDisplayedIndexByStep(startIndex, step, desiredStepCount) {
        if (startIndex < 0 || desiredStepCount <= 0)
            return -1;

        let stepsTaken = 0;
        let lastCandidateIndex = -1;

        for (let nextIndex = startIndex + step; nextIndex >= 0 && nextIndex < chat.count; nextIndex += step) {
            if (displayedEventHiddenAt(nextIndex))
                continue;

            const eventId = displayedEventIdAt(nextIndex);
            if (!eventId)
                continue;

            lastCandidateIndex = nextIndex;
            stepsTaken += 1;
            if (stepsTaken >= desiredStepCount)
                break;
        }

        return lastCandidateIndex;
    }

    function firstDisplayedNonHiddenEventIndex() {
        for (let index = 0; index < chat.count; index++) {
            if (displayedEventHiddenAt(index))
                continue;

            const eventId = displayedEventIdAt(index);
            if (eventId)
                return index;
        }

        return -1;
    }

    function lastDisplayedNonHiddenEventIndex() {
        for (let index = chat.count - 1; index >= 0; index--) {
            if (displayedEventHiddenAt(index))
                continue;

            const eventId = displayedEventIdAt(index);
            if (eventId)
                return index;
        }

        return -1;
    }

    function walkModeChunkSize() {
        const visibleCount = visibleWalkModeDelegatesInViewport().length;
        return Math.max(1, Math.floor(visibleCount / walkModeChunkDivisor));
    }

    function maybePrefetchOlderTimelineForWalk(targetIndex) {
        if (!walkModeActive || targetIndex < 0)
            return false;

        const olderDisplayedCount = (chat.count - 1) - targetIndex;
        if (olderDisplayedCount > walkModeOlderPrefetchThresholdItems)
            return false;

        return paginationController.requestMoreForOlderKeyboardWalk();
    }

    function messageInfoForEventId(eventId) {
        const index = displayedIndexForEventId(eventId);
        if (index < 0 || !chat.model || typeof chat.model.dataByIndex !== "function")
            return null;

        if (displayedEventHiddenAt(index))
            return null;

        function roleValue(role, fallbackValue) {
            const value = chat.model.dataByIndex(index, role);
            return value === undefined || value === null ? fallbackValue : value;
        }

        return {
            "eventId": String(eventId),
            "threadId": String(roleValue(Room.ThreadId, "") || ""),
            "type": Number(roleValue(Room.Type, -1)),
            "isSender": !!roleValue(Room.IsSender, false),
            "isEncrypted": !!roleValue(Room.IsEncrypted, false),
            "isEditable": !!roleValue(Room.IsEditable, false),
            "isStateEvent": !!roleValue(Room.IsStateEvent, false),
            "body": String(roleValue(Room.Body, "") || "")
        };
    }

    function primaryActionMessageInfo() {
        if (!primaryActionEventId)
            return null;

        return messageInfoForEventId(primaryActionEventId);
    }

    function closeKeyboardActions(options) {
        const control = keyboardActionsControl();

        pendingKeyboardActionsEventId = "";
        if (!control || !control.keyboardActive)
            return false;

        control.dismiss();
        if (!(options && options.skipTimelineFocus))
            focusTimelineSelection();
        return true;
    }

    function clearWalkState(options) {
        const shouldFocusComposer = !!(options && options.focusComposer);

        closeKeyboardActions({
            "skipTimelineFocus": true
        });
        pendingKeyboardActionsEventId = "";
        clearSelectedEvents();
        clearFocusedEvent();
        walkModeActive = false;

        if (shouldFocusComposer && composerAvailable) {
            Qt.callLater(function () {
                TimelineManager.focusMessageInput();
            });
        }
    }

    function exitWalkMode(options) {
        if (!walkModeActive && !hasFocusedEvent && !hasSelectedEvents && !keyboardActionsOpen)
            return false;

        clearWalkState(options);
        return true;
    }

    function reconcileWalkState(options) {
        const nextSelectedEventIds = [];

        for (let index = 0; index < selectedEventIds.length; index++) {
            const eventId = String(selectedEventIds[index] || "");
            const displayedIndex = displayedIndexForEventId(eventId);
            if (displayedIndex < 0 || displayedEventHiddenAt(displayedIndex))
                continue;

            nextSelectedEventIds.push(eventId);
        }

        const normalizedSelectedEventIds = normalizedEventIds(nextSelectedEventIds);
        if (!sameEventIdList(selectedEventIds, normalizedSelectedEventIds))
            selectedEventIds = normalizedSelectedEventIds;
        updateSelectionAnchor(selectionAnchorEventId);

        if (pendingKeyboardActionsEventId) {
            const pendingIndex = displayedIndexForEventId(pendingKeyboardActionsEventId);
            if (pendingIndex < 0
                    || displayedEventHiddenAt(pendingIndex)
                    || pendingKeyboardActionsEventId !== primaryActionEventId)
                pendingKeyboardActionsEventId = "";
        }

        if (!hasFocusedEvent) {
            if (walkModeActive || hasSelectedEvents)
                clearWalkState(options);
            return false;
        }

        const focusedIndex = displayedIndexForEventId(focusedEventId);
        if (focusedIndex < 0 || displayedEventHiddenAt(focusedIndex)) {
            clearWalkState(options);
            return false;
        }

        if (keyboardActionsOpen && !primaryActionEventId)
            closeKeyboardActions();

        return true;
    }

    function replaceTrackedEventId(oldId, newId) {
        const previousId = String(oldId || "");
        const nextId = String(newId || "");
        if (!previousId || !nextId || previousId === nextId)
            return;

        if (focusedEventId === previousId)
            focusedEventId = nextId;

        if (pendingKeyboardActionsEventId === previousId)
            pendingKeyboardActionsEventId = nextId;

        if (selectedEventIdsContains(previousId)) {
            const replacedEventIds = selectedEventIds.map(function (eventId) {
                return eventId === previousId ? nextId : eventId;
            });
            selectedEventIds = normalizedEventIds(replacedEventIds);
        }
    }

    function moveFocusByStep(step) {
        const currentIndex = displayedIndexForEventId(focusedEventId);
        if (currentIndex < 0)
            return false;

        const targetIndex = findDisplayedIndexByStep(currentIndex, step, 1);
        if (targetIndex >= 0)
            return focusDisplayedIndex(targetIndex, {
                "prefetchOlder": step > 0
            });

        if (step > 0)
            maybePrefetchOlderTimelineForWalk(currentIndex);

        return false;
    }

    function moveFocusTowardOlderEvents() {
        return moveFocusByStep(1);
    }

    function moveFocusTowardNewerEvents() {
        return moveFocusByStep(-1);
    }

    function moveFocusByChunk(step) {
        const currentIndex = displayedIndexForEventId(focusedEventId);
        if (currentIndex < 0)
            return false;

        const targetIndex = findDisplayedIndexByStep(currentIndex, step, walkModeChunkSize());
        if (targetIndex >= 0)
            return focusDisplayedIndex(targetIndex, {
                "prefetchOlder": step > 0
            });

        if (step > 0)
            maybePrefetchOlderTimelineForWalk(currentIndex);

        return false;
    }

    function moveFocusTowardOlderEventsByChunk() {
        return moveFocusByChunk(1);
    }

    function moveFocusTowardNewerEventsByChunk() {
        return moveFocusByChunk(-1);
    }

    function enterWalkModeAndMoveTowardOlderEventsByChunk() {
        if (!walkModeActive) {
            if (!enterWalkModeFromBottomMostVisible())
                return false;
        }

        return moveFocusTowardOlderEventsByChunk() || walkModeActive;
    }

    function focusOldestLoadedWalkModeEvent() {
        const index = lastDisplayedNonHiddenEventIndex();
        if (index < 0)
            return false;

        return focusDisplayedIndex(index, {
            "prefetchOlder": true
        });
    }

    function focusLatestWalkModeEvent() {
        const index = firstDisplayedNonHiddenEventIndex();
        if (index < 0)
            return false;

        return focusDisplayedIndex(index);
    }

    function openKeyboardActionsForPrimaryEvent() {
        if (!walkModeActive || !primaryActionEventId)
            return false;

        if (selectedCount > 1)
            return false;

        focusTimelineSelection();

        const delegate = visibleTimelineDelegates[primaryActionEventId] || null;
        if (!delegate) {
            pendingKeyboardActionsEventId = primaryActionEventId;
            scrollDisplayedIndexIntoView(displayedIndexForEventId(primaryActionEventId));
            return false;
        }

        pendingKeyboardActionsEventId = "";
        delegate.openKeyboardMessageActions();
        Qt.callLater(function () {
            const control = keyboardActionsControl();
            if (control)
                control.focusFirstVisibleButton();
        });
        return true;
    }

    function activateFocusedKeyboardAction() {
        const control = keyboardActionsControl();
        if (!control || !control.keyboardActive)
            return false;

        return control.activateFocusedButton();
    }

    function moveKeyboardActionsFocus(step) {
        const control = keyboardActionsControl();
        if (!control || !control.keyboardActive)
            return false;

        return control.moveFocus(step);
    }

    function keyboardActionsUseVerticalMovement() {
        const control = keyboardActionsControl();
        if (!control || !control.keyboardActive || typeof control.usesTwoRowLayout !== "function")
            return false;

        return control.usesTwoRowLayout();
    }

    function focusFirstKeyboardAction() {
        const control = keyboardActionsControl();
        if (!control || !control.keyboardActive)
            return false;

        return control.focusFirstVisibleButton();
    }

    function focusLastKeyboardAction() {
        const control = keyboardActionsControl();
        if (!control || !control.keyboardActive)
            return false;

        return control.focusLastVisibleButton();
    }

    function tryOpenPendingKeyboardActions() {
        if (!pendingKeyboardActionsEventId || pendingKeyboardActionsEventId !== primaryActionEventId)
            return false;

        if (!visibleTimelineDelegates[pendingKeyboardActionsEventId])
            return false;

        return openKeyboardActionsForPrimaryEvent();
    }

    function enterWalkModeFromBottomMostVisible() {
        if (!composerAvailable
                || disableTimelineList
                || roomSearchHasFocus
                || !roommodel
                || roommodel.input.uploads.length > 0
                || roommodel.reply
                || roommodel.edit
                || roommodel.thread)
            return false;

        const delegate = bottomMostVisibleDelegate();
        if (!delegate || !delegate.eventId)
            return false;

        clearSelectedEvents();
        return focusWalkModeEventById(String(delegate.eventId));
    }

    function handleMouseSelectionToggle(eventId) {
        const normalizedEventId = String(eventId || "");
        if (!normalizedEventId || !canExplicitlySelectEventId(normalizedEventId))
            return false;

        if (!walkModeActive) {
            if (!clearSelectedEvents() && selectionAnchorEventId.length > 0)
                selectionAnchorEventId = "";
        }

        if (!focusWalkModeEventById(normalizedEventId, {
                "skipScroll": true
            })) {
            return false;
        }

        return toggleSelectionForEventId(normalizedEventId);
    }

    function toggleFocusedEventSelection() {
        if (!walkModeActive || !focusedEventId)
            return false;

        return toggleSelectionForEventId(focusedEventId);
    }

    function openPrimaryMessageActionsDialog() {
        const message = primaryActionMessageInfo();
        if (!message)
            return false;

        closeKeyboardActions({
            "skipTimelineFocus": true
        });
        return messageActionSupport.openOptionsDialog(chatRoot, message);
    }

    function openWalkModeHelpDialog() {
        var helpDialog = createCatalogDialog(componentCatalog.timelineSelectionModeHelpDialog, {
                "appRoot": dialogHost || chatRoot
            });
        if (!helpDialog)
            return false;

        helpDialog.open();
        destroyOnClose(helpDialog);
        return true;
    }

    function performWalkModeAction(actionName) {
        if (selectedCount > 1)
            return false;

        const message = primaryActionMessageInfo();
        if (!message)
            return false;

        switch (actionName) {
        case "reply":
            if (!messageActionSupport.canReply(message, room))
                return false;
            break;
        case "thread":
            if (!messageActionSupport.canThread(message, room))
                return false;
            break;
        case "edit":
            if (!messageActionSupport.canEdit(message, room))
                return false;
            break;
        case "forward":
            if (!messageActionSupport.canForward(message))
                return false;
            break;
        case "remove":
            if (!messageActionSupport.canRemove(message, room))
                return false;
            break;
        case "raw":
            if (!messageActionSupport.canViewRaw(message))
                return false;
            break;
        default:
            break;
        }

        const exitsToComposer = actionName === "reply" || actionName === "thread" || actionName === "edit";

        if (exitsToComposer)
            exitWalkMode({
                "focusComposer": false
            });

        let handled = false;
        switch (actionName) {
        case "reply":
            handled = messageActionSupport.applyReply(room, message);
            break;
        case "thread":
            handled = messageActionSupport.applyThread(room, message);
            break;
        case "edit":
            handled = messageActionSupport.applyEdit(room, message);
            break;
        case "forward":
            handled = messageActionSupport.applyForward(chatRoot, message);
            break;
        case "remove":
            handled = messageActionSupport.applyRemove(chatRoot, room, message);
            break;
        case "raw":
            handled = messageActionSupport.applyViewRaw(room, message);
            break;
        case "options":
            handled = messageActionSupport.openOptionsDialog(chatRoot, message);
            break;
        default:
            handled = false;
        }

        return handled;
    }

    function handleEscape() {
        if (keyboardActionsOpen) {
            closeKeyboardActions();
            return true;
        }

        if (walkModeActive && selectedCount > 0) {
            clearSelectedEvents();
            focusTimelineSelection();
            return true;
        }

        if (walkModeActive) {
            exitWalkMode({
                "focusComposer": true
            });
            return true;
        }

        if (roommodel && roommodel.input.uploads.length > 0) {
            roommodel.input.declineUploads();
            return true;
        }

        if (roommodel && roommodel.reply) {
            roommodel.reply = undefined;
            return true;
        }

        if (roommodel && roommodel.edit) {
            roommodel.edit = undefined;
            return true;
        }

        if (roommodel && roommodel.thread) {
            roommodel.thread = undefined;
            return true;
        }

        if (roomSearchHasFocus)
            return false;

        if (!composerAvailable)
            return false;

        Qt.callLater(function () {
            TimelineManager.focusMessageInput();
        });
        return true;
    }

    function eventUsesNoWalkModeModifiers(event) {
        const modifiers = Number(event.modifiers);
        return (modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier | Qt.ShiftModifier)) === 0;
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

    function eventUsesShiftOnlyWalkModeModifiers(event) {
        const modifiers = Number(event.modifiers);
        return (modifiers & Qt.ShiftModifier) !== 0
            && (modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier)) === 0;
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
            || (eventMatchesWalkModeLatinKey(event, LayoutAgnosticKeys.LatinKey.O) && eventUsesWalkModeModifiers(event));
    }

    function isWalkModeHelpKey(event) {
        if (!event)
            return false;

        const text = String(event.text || "");
        const modifiers = Number(event.modifiers);
        return text === "?" && (modifiers & (Qt.ControlModifier | Qt.AltModifier | Qt.MetaModifier)) === 0;
    }

    function isRecognizedWalkModeKey(event) {
        return (event.key === Qt.Key_Escape)
            || (event.key === Qt.Key_Left && eventUsesWalkModeModifiers(event))
            || (event.key === Qt.Key_Right && eventUsesWalkModeModifiers(event))
            || (event.key === Qt.Key_Up && eventUsesWalkModeModifiers(event))
            || (event.key === Qt.Key_Down && eventUsesWalkModeModifiers(event))
            || (eventMatchesWalkModeLatinKey(event, LayoutAgnosticKeys.LatinKey.J) && eventUsesWalkModeModifiers(event))
            || (eventMatchesWalkModeLatinKey(event, LayoutAgnosticKeys.LatinKey.K) && eventUsesWalkModeModifiers(event))
            || (event.key === Qt.Key_Space && eventUsesWalkModeModifiers(event))
            || (eventMatchesWalkModeLatinKey(event, LayoutAgnosticKeys.LatinKey.R) && eventUsesWalkModeModifiers(event))
            || (eventMatchesWalkModeLatinKey(event, LayoutAgnosticKeys.LatinKey.T) && eventUsesWalkModeModifiers(event))
            || (eventMatchesWalkModeLatinKey(event, LayoutAgnosticKeys.LatinKey.E) && eventUsesWalkModeModifiers(event))
            || (eventMatchesWalkModeLatinKey(event, LayoutAgnosticKeys.LatinKey.F) && eventUsesWalkModeModifiers(event))
            || (eventMatchesWalkModeLatinKey(event, LayoutAgnosticKeys.LatinKey.D) && eventUsesWalkModeModifiers(event))
            || (eventMatchesWalkModeLatinKey(event, LayoutAgnosticKeys.LatinKey.U) && eventUsesWalkModeModifiers(event))
            || (eventMatchesWalkModeLatinKey(event, LayoutAgnosticKeys.LatinKey.I) && eventUsesWalkModeModifiers(event))
            || (eventMatchesWalkModeLatinKey(event, LayoutAgnosticKeys.LatinKey.D) && eventUsesCtrlWalkModeModifiers(event))
            || (eventMatchesWalkModeLatinKey(event, LayoutAgnosticKeys.LatinKey.U) && eventUsesCtrlWalkModeModifiers(event))
            || (eventMatchesWalkModeLatinKey(event, LayoutAgnosticKeys.LatinKey.G) && eventUsesWalkModeModifiers(event))
            || isWalkModeHelpKey(event)
            || isWalkModeEnterKey(event)
            || isWalkModeOptionsKey(event);
    }

    function isPrintableWalkModeText(event) {
        if (!eventUsesWalkModeModifiers(event) || isWalkModeEnterKey(event))
            return false;

        const text = String(event.text || "");
        return text.length > 0;
    }

    function handleWalkModeKey(event) {
        if (!event)
            return false;

        const gKeyPressed = eventMatchesWalkModeLatinKey(event, LayoutAgnosticKeys.LatinKey.G);
        const plainGPressed = gKeyPressed && eventUsesNoWalkModeModifiers(event);
        const shiftGPressed = gKeyPressed && eventUsesShiftOnlyWalkModeModifiers(event);

        if (keyboardActionsOpen) {
            if (!plainGPressed)
                resetWalkModeGoToTopSequence();

            if ((event.key === Qt.Key_Left
                        || eventMatchesWalkModeLatinKey(event, LayoutAgnosticKeys.LatinKey.H))
                    && eventUsesWalkModeModifiers(event)) {
                moveKeyboardActionsFocus(-1);
                event.accepted = true;
                return true;
            }

            if ((event.key === Qt.Key_Right
                        || eventMatchesWalkModeLatinKey(event, LayoutAgnosticKeys.LatinKey.L))
                    && eventUsesWalkModeModifiers(event)) {
                moveKeyboardActionsFocus(1);
                event.accepted = true;
                return true;
            }

            if (keyboardActionsUseVerticalMovement()
                    && (event.key === Qt.Key_Up || eventMatchesWalkModeLatinKey(event, LayoutAgnosticKeys.LatinKey.K))
                    && eventUsesWalkModeModifiers(event)) {
                moveKeyboardActionsFocus(-1);
                event.accepted = true;
                return true;
            }

            if (keyboardActionsUseVerticalMovement()
                    && (event.key === Qt.Key_Down || eventMatchesWalkModeLatinKey(event, LayoutAgnosticKeys.LatinKey.J))
                    && eventUsesWalkModeModifiers(event)) {
                moveKeyboardActionsFocus(1);
                event.accepted = true;
                return true;
            }

            if (shiftGPressed) {
                focusLastKeyboardAction();
                event.accepted = true;
                return true;
            }

            if (plainGPressed) {
                if (pendingWalkModeGoToTopRequest) {
                    resetWalkModeGoToTopSequence();
                    focusFirstKeyboardAction();
                } else {
                    pendingWalkModeGoToTopRequest = true;
                    walkModeGoToTopSequenceTimer.restart();
                }
                event.accepted = true;
                return true;
            }

            if (isWalkModeEnterKey(event) && eventUsesWalkModeModifiers(event)) {
                activateFocusedKeyboardAction();
                event.accepted = true;
                return true;
            }

            if (event.key === Qt.Key_Escape) {
                handleEscape();
                event.accepted = true;
                return true;
            }

            if (isRecognizedWalkModeKey(event) || isPrintableWalkModeText(event)) {
                event.accepted = true;
                return true;
            }

            return false;
        }

        if (!walkModeActive)
            return false;

        const walkBarFocused = selectionModeBarHasFocus();

        if (!plainGPressed)
            resetWalkModeGoToTopSequence();

        if ((event.key === Qt.Key_Left || eventMatchesWalkModeLatinKey(event, LayoutAgnosticKeys.LatinKey.H))
                && eventUsesWalkModeModifiers(event)) {
            if (walkBarFocused) {
                if (!moveSelectionModeBarFocus(-1))
                    focusTimelineSelection();
            } else {
                focusLastSelectionModeBarButton();
            }
            event.accepted = true;
            return true;
        }

        if ((event.key === Qt.Key_Right || eventMatchesWalkModeLatinKey(event, LayoutAgnosticKeys.LatinKey.L))
                && eventUsesWalkModeModifiers(event)) {
            if (walkBarFocused) {
                moveSelectionModeBarFocus(1);
            } else {
                focusFirstSelectionModeBarButton();
            }
            event.accepted = true;
            return true;
        }

        if ((event.key === Qt.Key_Up || eventMatchesWalkModeLatinKey(event, LayoutAgnosticKeys.LatinKey.K))
                && eventUsesWalkModeModifiers(event)) {
            moveFocusTowardOlderEvents();
            event.accepted = true;
            return true;
        }

        if ((event.key === Qt.Key_Down || eventMatchesWalkModeLatinKey(event, LayoutAgnosticKeys.LatinKey.J))
                && eventUsesWalkModeModifiers(event)) {
            moveFocusTowardNewerEvents();
            event.accepted = true;
            return true;
        }

        if (event.key === Qt.Key_Space && eventUsesWalkModeModifiers(event)) {
            toggleFocusedEventSelection();
            event.accepted = true;
            return true;
        }

        if (eventMatchesWalkModeLatinKey(event, LayoutAgnosticKeys.LatinKey.U) && eventUsesCtrlWalkModeModifiers(event)) {
            moveFocusTowardOlderEventsByChunk();
            event.accepted = true;
            return true;
        }

        if (eventMatchesWalkModeLatinKey(event, LayoutAgnosticKeys.LatinKey.D) && eventUsesCtrlWalkModeModifiers(event)) {
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
            if (pendingWalkModeGoToTopRequest) {
                resetWalkModeGoToTopSequence();
                focusOldestLoadedWalkModeEvent();
            } else {
                pendingWalkModeGoToTopRequest = true;
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

        if (isWalkModeEnterKey(event) && eventUsesWalkModeModifiers(event)) {
            openKeyboardActionsForPrimaryEvent();
            event.accepted = true;
            return true;
        }

        if (isWalkModeOptionsKey(event)) {
            openPrimaryMessageActionsDialog();
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

        if (eventMatchesWalkModeLatinKey(event, LayoutAgnosticKeys.LatinKey.I) && eventUsesWalkModeModifiers(event)) {
            exitWalkMode({
                "focusComposer": true
            });
            event.accepted = true;
            return true;
        }

        if (event.key === Qt.Key_Escape) {
            handleEscape();
            event.accepted = true;
            return true;
        }

        if (isPrintableWalkModeText(event)) {
            event.accepted = true;
            return true;
        }

        return false;
    }

    function scheduleTimelineModelBinding() {
        roomSwitchBindSerial += 1;
        const targetRoom = roommodel;

        if (!targetRoom || disableTimelineList) {
            clearWalkState({
                "focusComposer": false
            });
            resetVisibleDelegateRegistry();
            pendingRoomModel = null;
            activeRoomModel = null;
            roomSwitchInProgress = false;
            return;
        }

        pendingRoomModel = targetRoom;
        roomSwitchInProgress = true;

        if (TimelineManager.roomSwitchPerfEnabled())
            TimelineManager.markRoomSwitchPhase(targetRoom.roomId, "qml.message_view.bind_scheduled");

        timelineBindDelay.restart();
    }

    function bindPendingTimelineModel() {
        const targetRoom = pendingRoomModel;

        if (!targetRoom || disableTimelineList || roommodel !== targetRoom)
            return;

        if (TimelineManager.roomSwitchPerfEnabled())
            TimelineManager.markRoomSwitchPhase(targetRoom.roomId, "qml.message_view.model_bind_begin");

        activeRoomModel = targetRoom;

        if (TimelineManager.roomSwitchPerfEnabled())
            TimelineManager.markRoomSwitchPhase(targetRoom.roomId, "qml.message_view.model_bound");

        if (chat.count === 0)
            roomSwitchInProgress = false;

        paginationController.onBindCompleted();
    }

    onRoommodelChanged: {
        clearWalkState({
            "focusComposer": false
        });
        resetVisibleDelegateRegistry();
        scheduleTimelineModelBinding();
    }
    onDisableTimelineListChanged: scheduleTimelineModelBinding()
    onRoomSearchHasFocusChanged: {
        if (roomSearchHasFocus && walkModeActive) {
            exitWalkMode({
                "focusComposer": false
            });
        }
    }

    Component.onCompleted: scheduleTimelineModelBinding()

    Timer {
        id: walkModeGoToTopSequenceTimer

        interval: 400
        repeat: false
        onTriggered: chatRoot.pendingWalkModeGoToTopRequest = false
    }

    Timer {
        id: timelineBindDelay

        interval: 16
        repeat: false
        onTriggered: chatRoot.bindPendingTimelineModel()
    }

    function destroyOnClose(dialog) {
        if (!dialog)
            return;

        if (dialogHost && dialogHost.destroyOnClose != undefined) {
            dialogHost.destroyOnClose(dialog);
            return;
        }

        if (dialog.closing != undefined)
            dialog.closing.connect(() => dialog.destroy(1000));
        else if (dialog.aboutToHide != undefined)
            dialog.aboutToHide.connect(() => dialog.destroy(1000));
    }

    function createCatalogDialog(componentUrl, properties) {
        if (!dialogHost || !componentUrl)
            return null;

        if (dialogHost.createDialog != undefined)
            return dialogHost.createDialog(componentUrl, properties || {});

        var component = Qt.createComponent(componentUrl);
        if (component.status !== Component.Ready) {
            console.error("Failed to create component: " + component.errorString());
            return null;
        }

        var dialog = component.createObject(dialogHost, properties || {});
        if (!dialog)
            console.error("Failed to create dialog object for: " + componentUrl);
        return dialog;
    }

    function openForwardDialog(eventId) {
        if (!eventId)
            return null;

        if (dialogHost && dialogHost.showForwardMessageDialog != undefined)
            return dialogHost.showForwardMessageDialog(room, eventId, timeline, timelineView);

        var forwardDialog = createCatalogDialog(componentCatalog.navigationForwardCompleterDialog, {
                "roomSource": room,
                "timelineSource": timeline ?? null,
                "timelineViewSource": timelineView ?? null,
                "showReplyPreview": !!timeline && !!timelineView
            });
        if (!forwardDialog)
            return null;
        forwardDialog.setMessageEventId(eventId);
        forwardDialog.open();
        destroyOnClose(forwardDialog);
        return forwardDialog;
    }

    function clearSearch() {
        if (typeof topBar !== "undefined" && topBar) {
            topBar.searchString = "";
            return;
        }

        searchString = "";
    }

    function showDialogFromComponent(componentRef, properties) {
        var dialogParent = dialogHost || chatRoot;
        var dialog = componentRef.createObject(dialogParent, properties || {});
        if (!dialog)
            return null;
        dialog.open();
        destroyOnClose(dialog);
        return dialog;
    }

    // HACK: https://bugreports.qt.io/browse/QTBUG-83972, qtwayland cannot auto hide menu
    Connections {
        function onHideMenu() {
            messageContextMenuC.close();
            replyContextMenuC.close();
        }

        target: MainWindow
    }

    Connections {
        function onScrollToIndex(index) {
            chat.keepPinnedToBottom = false;
            chat.positionViewAtIndex(index, ListView.Center);
            chat.updateLastScroll();
        }

        target: room
    }

    ScrollBar {
        id: scrollbar

        readonly property int scrollbarPolicy: Settings.uiScrollbarPolicy
        readonly property bool scrollbarVisible: {
            switch (scrollbarPolicy) {
            case Settings.ScrollbarPolicy.Always:
                return true;
            case Settings.ScrollbarPolicy.Never:
                return false;
            case Settings.ScrollbarPolicy.WhenNeeded:
            default:
                return chat.contentHeight > chat.height;
            }
        }
        policy: scrollbarVisible ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.top: parent.top
        parent: chat.parent
    }

    TimelinePaginationController {
        id: paginationController

        chatList: chat
        scrollbar: scrollbar
        activeRoomModel: chatRoot.activeRoomModel
        roomModel: chatRoot.roommodel
        disableTimelineList: chatRoot.disableTimelineList
        filteringRequested: chatRoot.filteringRequested
        roomSwitchInProgress: chatRoot.roomSwitchInProgress
    }

    Shortcut {
        enabled: chatRoot.selectionModeEnterShortcutEnabled
        sequences: ["Return", "Enter"]
        context: Qt.ApplicationShortcut

        onActivated: {
            if (chatRoot.keyboardActionsOpen)
                chatRoot.activateFocusedKeyboardAction();
            else
                chatRoot.openKeyboardActionsForPrimaryEvent();
        }
        onActivatedAmbiguously: {
            if (chatRoot.keyboardActionsOpen)
                chatRoot.activateFocusedKeyboardAction();
            else
                chatRoot.openKeyboardActionsForPrimaryEvent();
        }
    }

    ListView {
        id: chat

        property int delegateMaxWidth: ((Settings.uiLayoutContentMaxWidthEffectivePx > 0 && Settings.uiLayoutContentMaxWidthEffectivePx < chatRoot.availableWidth) ? Settings.uiLayoutContentMaxWidthEffectivePx : chatRoot.availableWidth) - chatRoot.padding * 2 - (scrollbar.interactive ? scrollbar.width : 0)

        KeyNavigation.tab: chatRoot.walkModeActive && !chatRoot.keyboardActionsOpen
            ? chatRoot.firstSelectionModeBarTabTarget()
            : null
        KeyNavigation.backtab: chatRoot.walkModeActive && !chatRoot.keyboardActionsOpen
            ? chatRoot.lastRoomHeaderActionButtonTarget()
            : null
        KeyNavigation.priority: KeyNavigation.BeforeItem
        Keys.priority: Keys.BeforeItem
        Keys.onPressed: event => chatRoot.handleWalkModeKey(event)
        ScrollBar.vertical: scrollbar
        anchors.fill: parent
        anchors.rightMargin: scrollbar.interactive ? scrollbar.width : 0
        // reuseItems had bugs in older Qt (QTBUG-95105, QTBUG-95107).
        // Re-enabled experimentally on Qt 6.10+ to reduce delegate churn and memory fragmentation.
        reuseItems: true
        boundsBehavior: Flickable.StopAtBounds

        // Boost mouse-wheel scroll speed: Qt Quick's default Flickable wheel
        // handling scrolls ~60px per notch, which is too sluggish for a
        // timeline with large message delegates. This WheelHandler intercepts
        // wheel events and applies a larger per-notch delta directly.
        WheelHandler {
            orientation: Qt.Vertical
            acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad

            property real _prevRotation: 0
            onRotationChanged: {
                let delta = rotation - _prevRotation;
                _prevRotation = rotation;
                // Each wheel notch ≈ 15° of rotation.
                // Scale to ~150px per notch for comfortable timeline scrolling.
                chat.contentY -= delta * 5;
                chat.returnToBounds();
                // WheelHandler bypasses ListView's movement lifecycle, so
                // keepPinnedToBottom must be maintained manually here.
                chat.updateLastScroll();
                chat.keepPinnedToBottom = !chatRoot.filteringRequested && chat.atYEnd;
            }
        }
        // Keep initial room-switch render cheap by avoiding extra off-screen delegate creation
        // until first content is visible.
        displayMarginBeginning: chatRoot.listViewDisplayMargin
        displayMarginEnd: chatRoot.listViewDisplayMargin
        cacheBuffer: chatRoot.listViewCacheBuffer
        model: chatRoot.disableTimelineList
            ? null
            : (chatRoot.filteringRequested ? filteredTimeline : chatRoot.activeRoomModel)
        //pixelAligned: true
        spacing: Komai.uiLayoutCompactMode ? 2 : Math.round(1.5 * Komai.paddingSmall)
        verticalLayoutDirection: ListView.BottomToTop

        property real lastScrollPos: 0
        property bool keepPinnedToBottom: true

        // Fixup the scroll position when the height changes. Without this, the view is kept around the center of the currently visible content, while we usually want to stick to the bottom.
        function updateLastScroll() {
            lastScrollPos = (contentY+height);
        }
        onMovementEnded: {
            updateLastScroll();
            keepPinnedToBottom = !chatRoot.filteringRequested && atYEnd;
            paginationController.onMovementEnded();
        }
        onMovementStarted: {
            paginationController.onMovementStarted();
        }
        onModelChanged: {
            chatRoot.perfFirstVisibleItemLogged = false;
            updateLastScroll();
            keepPinnedToBottom = !chatRoot.filteringRequested && atYEnd;
            paginationController.onTimelineModelChanged();
            if (!model) {
                chatRoot.clearWalkState({
                    "focusComposer": false
                });
                chatRoot.resetVisibleDelegateRegistry();
                return;
            }
            Qt.callLater(function () {
                chatRoot.reconcileWalkState({
                    "focusComposer": false
                });
                chatRoot.tryOpenPendingKeyboardActions();
            });
        }
        onAtYBeginningChanged: paginationController.onAtYBeginningChanged(atYBeginning)
        onHeightChanged: {
            contentY = (lastScrollPos-height);
            if (keepPinnedToBottom && !chatRoot.filteringRequested)
                positionViewAtBeginning();
            paginationController.scheduleNeededPagination();
        }
        onContentHeightChanged: {
            if (keepPinnedToBottom && !chatRoot.filteringRequested && !moving && !flicking && !dragging) {
                positionViewAtBeginning();
                updateLastScroll();
            }
            paginationController.scheduleNeededPagination();
        }
        Component.onCompleted: {
            updateLastScroll();
            keepPinnedToBottom = !chatRoot.filteringRequested && atYEnd;
        }

        Component {
            id: plainMessageStyle

            TimelinePlainMessageStyle {
                messageActions: messageActionsHost.control
                messageContextMenu: messageContextMenuC
                replyContextMenu: replyContextMenuC
                scrolledToThis: eventId === room.scrollTarget && (y + height > chat.y + chat.contentY && y < chat.y + chat.height + chat.contentY)
                data: [
                    Connections {
                        function onMovementEnded() {
                            if (y + height + 2 * chat.spacing > chat.contentY + chat.height && y < chat.contentY + chat.height) {
                                room.currentIndex = index;
                            }
                        }
                        target: chat
                    }
                ]
            }
        }
        Component {
            id: bubbleMessageStyle

            TimelineBubbleMessageStyle {
                messageActions: messageActionsHost.control
                messageContextMenu: messageContextMenuC
                replyContextMenu: replyContextMenuC
                scrolledToThis: eventId === room.scrollTarget && (y + height > chat.y + chat.contentY && y < chat.y + chat.height + chat.contentY)
                data: [
                    Connections {
                        function onMovementEnded() {
                            if (y + height + 2 * chat.spacing > chat.contentY + chat.height && y < chat.contentY + chat.height) {
                                room.currentIndex = index;
                            }
                        }
                        target: chat
                    }
                ]
            }
        }

        function styleDelegateFor(style, _positioning) {
            switch (style) {
            case Settings.TimelineMessagesStyle.Bubbles:
                return bubbleMessageStyle;
            case Settings.TimelineMessagesStyle.Plain:
            default:
                return plainMessageStyle;
            }
        }

        delegate: styleDelegateFor(Settings.timelineMessagesStyle, Settings.timelineMessagesPositioning)
        footer: TimelineLoadingFooter {
            delegateWidth: chat.delegateMaxWidth
            roomModel: chatRoot.activeRoomModel
            filteringInProgress: chatRoot.filteringInProgress
            searchString: chatRoot.searchString
        }

        onCountChanged: {
            if (!chatRoot.perfFirstVisibleItemLogged && chatRoot.roomSwitchInProgress && count > 0 && roommodel) {
                chatRoot.perfFirstVisibleItemLogged = true;
                TimelineManager.markRoomSwitchPhase(roommodel.roomId,
                                                    "qml.message_view.first_visible_item");
            }
            if (chatRoot.roomSwitchInProgress && count > 0 && chatRoot.activeRoomModel && roommodel === chatRoot.activeRoomModel)
                chatRoot.roomSwitchInProgress = false;
            // Mark timeline as read
            if (atYEnd && model && room)
                model.currentIndex = 0;
            paginationController.onCountChanged();
            chatRoot.reconcileWalkState({
                "focusComposer": false
            });
            chatRoot.tryOpenPendingKeyboardActions();
        }

        TimelineFilter {
            id: filteredTimeline

            filterByContent: chatRoot.searchString
            filterByNotifications: chatRoot.filterByNotifications
            filterByThread: chatRoot.activeRoomModel ? chatRoot.activeRoomModel.thread : ""
            source: chatRoot.filteringRequested ? chatRoot.activeRoomModel : null
        }
        MessageActionsHost {
            id: messageActionsHost
            chatList: chat
            chatRoot: chatRoot
            emojiPopup: chatRoot.emojiPopup
            filteredTimeline: filteredTimeline
            roomModel: room
        }
        Connections {
            function onRowsInserted() {
                chatRoot.reconcileWalkState({
                    "focusComposer": false
                });
                chatRoot.tryOpenPendingKeyboardActions();
            }

            function onRowsRemoved() {
                chatRoot.reconcileWalkState({
                    "focusComposer": false
                });
            }

            function onLayoutChanged() {
                chatRoot.reconcileWalkState({
                    "focusComposer": false
                });
                chatRoot.tryOpenPendingKeyboardActions();
            }

            function onModelReset() {
                chatRoot.clearWalkState({
                    "focusComposer": false
                });
                chatRoot.resetVisibleDelegateRegistry();
            }

            target: chat.model ? chat.model : null
            ignoreUnknownSignals: true
        }
        Connections {
            function onActivationModeChanged() {
                chatRoot.keyboardActionsOpen = messageActionsHost.control.keyboardActive;
                if (chatRoot.keyboardActionsOpen && !chatRoot.walkModeActive && chatRoot.hasFocusedEvent)
                    chatRoot.walkModeActive = true;
            }

            target: messageActionsHost.control
        }
        Connections {
            function onEventIdReplaced(oldId, newId) {
                chatRoot.replaceTrackedEventId(oldId, newId);
            }

            target: chatRoot.activeRoomModel
        }
        TimelineKeyboardShortcuts {
            chatList: chat
            chatRoot: chatRoot
            roomModel: room
        }
    }
    MessageContextMenu {
        id: messageContextMenuC

        chatRoot: chatRoot
        emojiPopup: chatRoot.emojiPopup
        filteredTimelineModel: filteredTimeline
        roomModel: room
    }

    Component {
        id: removeReasonDialogComponent

        InputDialog {
            property string eventId

            placeholderText: qsTr("Optional reason")
            prompt: ""
            title: qsTr("Delete this message?")
            titleIcon: ":/icons/icons/ui/delete.svg"
            acceptText: qsTr("Delete")

            onInputAccepted: function (text) {
                room.redactEvent(eventId, text);
            }
        }
    }

    Component {
        id: reportMessageDialogComponent

        ReportMessage {
        }
    }

    function openMessageActionsDialog(eventId, threadId, eventType, isSender, isEncrypted, isEditable, link, text) {
        var component = Qt.createComponent("qrc:/resources/qml/dialogs/timeline/MessageActionsDialog.qml");
        if (component.status !== Component.Ready) {
            console.error("MessageActionsDialog: " + component.errorString());
            return;
        }
        var dialogParent = dialogHost || chatRoot;
        var dialog = component.createObject(dialogParent, {
            "eventId": eventId,
            "eventType": eventType,
            "isSender": isSender,
            "isEncrypted": isEncrypted,
            "link": link || "",
            "roomModel": room,
            "chatRoot": chatRoot,
            "appRoot": dialogParent
        });
        if (!dialog)
            return;
        dialog.open();
        destroyOnClose(dialog);
    }

    function openRemoveMessageDialog(eventId) {
        showDialogFromComponent(removeReasonDialogComponent, {
            "eventId": eventId
        });
    }

    function openReportMessageDialog(eventId) {
        showDialogFromComponent(reportMessageDialogComponent, {
            "eventId": eventId
        });
    }
    ReplyContextMenu {
        id: replyContextMenuC

        roomModel: room
    }
    TimelineToEndButton {
        chatList: chat
        scrollbarItem: scrollbar
    }

    Rectangle {
        anchors.fill: parent
        color: palette.base
        visible: chatRoot.roomSwitchInProgress
                 && !chatRoot.disableTimelineList
                 && !chatRoot.suppressRoomSwitchSpinner
        z: 20

        Spinner {
            anchors.centerIn: parent
            running: parent.visible
            visible: running
            height: Komai.timelineLogoSize
            z: 3
        }
    }
}
