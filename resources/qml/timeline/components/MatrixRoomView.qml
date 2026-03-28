// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../room/components"
import "../../composer" as Composer
import "../../dialogs/moderation" as ModerationDialogs
import "../../dialogs/navigation" as NavigationDialogs
import "../../dialogs/timeline" as TimelineDialogs
import "../styles/bubble"
import "../styles/plain"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import cc.etke.komai

ColumnLayout {
    id: root

    required property var roomPreview
    required property bool showBackButton
    property var chatRoot: null
    property var timelineRoot: null
    property var emojiPopup: null
    property var filteredTimeline: null
    property bool walkModeActive: false
    property string focusedEventId: ""
    property var selectedEventIds: []
    property string selectionAnchorEventId: ""
    property var visibleTimelineDelegates: ({})
    readonly property int selectedCount: selectedEventIds.length
    readonly property bool hasSelectedEvents: selectedCount > 0
    readonly property bool hasSingleSelection: selectedCount === 1
    readonly property string singleSelectedEventId: hasSingleSelection ? String(selectedEventIds[0] || "") : ""
    readonly property string primaryActionEventId: hasSingleSelection
        ? singleSelectedEventId
        : (!hasSelectedEvents ? focusedEventId : "")
    readonly property bool hasFocusedEvent: focusedEventId.length > 0

    readonly property bool hasTimeline: TimelineManager.matrixTimelineItemCount > 0
    readonly property bool loading: TimelineManager.matrixTimelineLoading
    readonly property int composerBaselineHeight: Math.max(48, Komai.navigationRowHeight)
    readonly property var composerShell: composerContainer
    readonly property var notificationAreaItem: timelineViewport
    readonly property int pendingAttachmentCount: TimelineManager.matrixTimelineAttachmentCount
    readonly property bool hasPendingAttachments: pendingAttachmentCount > 0
    readonly property string activeEditEventId: TimelineManager.matrixTimelineEditEventId
    readonly property bool editing: activeEditEventId.length > 0
    property string draftBeforeEdit: ""
    property bool restoringEditDraft: false
    property int lastPaginationTriggerCount: -1
    property int lastInitialBufferTriggerCount: -1
    property string activeRoomId: roomPreview ? String(roomPreview.roomid || "") : ""
    property var measuredTimelineHeights: ({})
    property bool initialBottomPinPending: false
    property bool initialTimelineBufferPending: false
    property bool deferredInitialBufferTopUpPending: false
    property bool bufferPaginationInFlight: false
    property bool perfLoggedCountNonZero: false
    property bool perfLoggedContentHeightReady: false
    property bool perfLoggedUsefulHeightReady: false
    property bool perfLoggedBufferFilled: false
    property bool suppressNextWalkModeOlderStep: false
    property string lastMarkedReadEventId: ""
    property bool preferLatestReadMarkerEvent: false
    readonly property var matrixUploadsController: roomSupport.uploadsController
    readonly property var matrixComposerInputController: roomSupport.composerInputController
    readonly property var matrixComposerRoom: roomSupport.composerRoom
    readonly property var matrixMessageActionsDefaultRoomModel: roomSupport.messageActionsDefaultRoomModel
    readonly property var matrixMessageContextMenu: roomSupport.messageContextMenu
    readonly property var matrixReplyContextMenu: roomSupport.replyContextMenu
    readonly property var matrixMessageActionsHost: roomSupport.messageActionsHost
    readonly property var matrixDialogRoomModel: roomSupport.dialogRoomModel
    readonly property var matrixForwardRoomModel: roomSupport.forwardRoomModel
    readonly property var matrixHeaderRoomModel: roomSupport.headerRoomModel

    MessageActionSupport {
        id: messageActionSupport
    }

    MatrixRoomSupport {
        id: roomSupport

        rootItem: root
        roomPreview: root.roomPreview
        chatRoot: root.chatRoot
        timelineRoot: root.timelineRoot
        emojiPopup: root.emojiPopup
        filteredTimeline: root.filteredTimeline
        timelineList: matrixTimelineList
    }

    // Debounce buffer-fill checks so the layout has time to settle
    // before we decide whether more items are needed.  Without this,
    // transient contentHeight values (e.g. 536 while 18 items are
    // still rendering) cause premature paginate requests.
    Timer {
        id: bufferCheckTimer
        interval: 35
        onTriggered: root.maybeRequestInitialTimelineBuffer()
    }

    Timer {
        id: deferredBufferCheckTimer
        interval: 140
        onTriggered: root.maybeRequestDeferredInitialTimelineBuffer()
    }

    Timer {
        id: walkModeEntrySuppressTimer

        interval: 0
        onTriggered: root.suppressNextWalkModeOlderStep = false
    }

    Timer {
        id: readMarkerUpdateTimer

        interval: 0
        onTriggered: root.updateReadMarkerForVisibleContent()
    }

    function clearSearch() {
        if (root.chatRoot && typeof root.chatRoot.clearSearch === "function")
            root.chatRoot.clearSearch();
    }

    function markRoomSwitchPerfPhase(phase) {
        if (!TimelineManager.roomSwitchPerfEnabled() || activeRoomId.length === 0 || phase.length === 0)
            return;

        TimelineManager.markRoomSwitchPhase(activeRoomId, phase);
    }

    function selectedEventIdsContains(eventId) {
        const normalizedEventId = String(eventId || "");
        return normalizedEventId.length > 0 && selectedEventIds.indexOf(normalizedEventId) >= 0;
    }

    function canExplicitlySelectEventId(eventId) {
        const normalizedEventId = String(eventId || "");
        return normalizedEventId.length > 0
            && TimelineManager.matrixTimelineModel
            && TimelineManager.matrixTimelineModel.rowForEventId(normalizedEventId) >= 0;
    }

    function updateSelectionAnchor(preferredEventId) {
        const normalizedEventId = String(preferredEventId || "");
        if (selectedEventIdsContains(normalizedEventId)) {
            selectionAnchorEventId = normalizedEventId;
            return;
        }

        selectionAnchorEventId = selectedEventIds.length > 0
            ? String(selectedEventIds[selectedEventIds.length - 1] || "")
            : "";
    }

    function toggleSelectionForEventId(eventId) {
        const normalizedEventId = String(eventId || "");
        if (!canExplicitlySelectEventId(normalizedEventId))
            return false;

        const wasSelected = selectedEventIdsContains(normalizedEventId);
        if (wasSelected) {
            selectedEventIds = selectedEventIds.filter(function (selectedEventId) {
                return String(selectedEventId || "") !== normalizedEventId;
            });
        } else {
            selectedEventIds = selectedEventIds.concat([normalizedEventId]);
        }

        updateSelectionAnchor(wasSelected ? "" : normalizedEventId);
        return true;
    }

    function registerVisibleDelegate(eventId, delegateItem) {
        const key = String(eventId || "");
        if (key.length === 0 || !delegateItem)
            return;

        visibleTimelineDelegates[key] = delegateItem;
        visibleTimelineDelegatesChanged();
    }

    function unregisterVisibleDelegate(eventId, delegateItem) {
        const key = String(eventId || "");
        if (key.length === 0)
            return;

        if (!visibleTimelineDelegates[key])
            return;
        if (delegateItem && visibleTimelineDelegates[key] !== delegateItem)
            return;

        delete visibleTimelineDelegates[key];
        visibleTimelineDelegatesChanged();
    }

    function replaceTrackedEventId(previousId, nextId) {
        const oldKey = String(previousId || "");
        const newKey = String(nextId || "");
        if (oldKey.length === 0 || newKey.length === 0 || oldKey === newKey)
            return;

        const tracked = visibleTimelineDelegates[oldKey];
        if (!tracked)
            return;

        delete visibleTimelineDelegates[oldKey];
        visibleTimelineDelegates[newKey] = tracked;
        visibleTimelineDelegatesChanged();

        if (focusedEventId === oldKey)
            focusedEventId = newKey;
        if (selectionAnchorEventId === oldKey)
            selectionAnchorEventId = newKey;
        if (selectedEventIds.indexOf(oldKey) >= 0) {
            selectedEventIds = selectedEventIds.map(function (eventId) {
                return String(eventId || "") === oldKey ? newKey : eventId;
            });
        }
    }

    function bottomMostVisibleDelegate() {
        const viewportTop = matrixTimelineList ? matrixTimelineList.contentY : 0;
        const viewportBottom = viewportTop + (matrixTimelineList ? matrixTimelineList.height : 0);
        let candidate = null;
        let candidateBottom = -1;

        for (const eventId in visibleTimelineDelegates) {
            const delegateItem = visibleTimelineDelegates[eventId];
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

    function latestLoadedEventId() {
        const model = TimelineManager.matrixTimelineModel;
        if (!model || TimelineManager.matrixTimelineItemCount <= 0)
            return "";

        for (let row = 0; row < TimelineManager.matrixTimelineItemCount; row++) {
            const latestItem = model.itemAt(row);
            if (!latestItem || latestItem === undefined)
                continue;

            const eventId = String(latestItem.eventId || "");
            if (eventId.length > 0)
                return eventId;
        }

        return "";
    }

    function latestLoadedSelectableEventId() {
        for (let row = 0; row < TimelineManager.matrixTimelineItemCount; row++) {
            if (!isSelectableMatrixTimelineRow(row))
                continue;

            const item = TimelineManager.matrixTimelineModel.itemAt(row);
            const eventId = String(item.eventId || "");
            if (eventId.length > 0)
                return eventId;
        }

        return "";
    }

    function isEffectivelyAtLiveEdge() {
        if (!matrixTimelineList)
            return false;

        if (matrixTimelineList.keepPinnedToBottom || root.initialBottomPinPending || matrixTimelineList.atYEnd)
            return true;

        if (matrixTimelineList.userUnpinned)
            return false;

        const viewportHeight = Number(matrixTimelineList.height || 0);
        const contentHeight = Number(matrixTimelineList.contentHeight || 0);
        return viewportHeight > 0 && contentHeight > 0 && contentHeight <= viewportHeight + 2;
    }

    function selectableEventIdNearMatrixRow(row) {
        const model = TimelineManager.matrixTimelineModel;
        const rowCount = TimelineManager.matrixTimelineItemCount;
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
            const itemKind = String(item.itemKind || "");
            if (eventId.length === 0 || itemKind === "date_divider")
                continue;

            return eventId;
        }

        return "";
    }

    function bottomMostVisibleEventId() {
        if (isEffectivelyAtLiveEdge()) {
            const latestEventId = latestLoadedSelectableEventId();
            if (latestEventId.length > 0)
                return latestEventId;
        }

        if (!matrixTimelineList || !TimelineManager.matrixTimelineModel || matrixTimelineList.width <= 0
                || matrixTimelineList.height <= 0) {
            const delegateItem = bottomMostVisibleDelegate();
            return delegateItem && delegateItem.eventId
                ? String(delegateItem.eventId || "")
                : "";
        }

        const probeX = Math.max(1, Math.round(matrixTimelineList.width / 2));
        const probeY = Math.max(1, Math.round(matrixTimelineList.height - 2));
        const row = matrixTimelineList.indexAt(probeX, probeY);
        if (row >= 0) {
            const eventId = selectableEventIdNearMatrixRow(row);
            if (eventId.length > 0)
                return eventId;
        }

        const delegateItem = bottomMostVisibleDelegate();
        if (delegateItem && delegateItem.eventId)
            return String(delegateItem.eventId || "");

        return "";
    }

    function scheduleReadMarkerUpdate(preferLatestEvent) {
        if (!root.visible || activeRoomId.length === 0 || !hasTimeline)
            return;

        preferLatestReadMarkerEvent = preferLatestReadMarkerEvent || !!preferLatestEvent;
        readMarkerUpdateTimer.restart();
    }

    function updateReadMarkerForVisibleContent() {
        if (!root.visible || activeRoomId.length === 0 || !hasTimeline) {
            preferLatestReadMarkerEvent = false;
            return;
        }

        let targetEventId = "";
        if (preferLatestReadMarkerEvent || isEffectivelyAtLiveEdge())
            targetEventId = latestLoadedSelectableEventId();
        if (targetEventId.length === 0)
            targetEventId = bottomMostVisibleEventId();

        preferLatestReadMarkerEvent = false;

        if (targetEventId.length === 0 || targetEventId === lastMarkedReadEventId)
            return;

        TimelineManager.markActiveMatrixTimelineEventAsRead(targetEventId);
        lastMarkedReadEventId = targetEventId;
    }

    function focusedDelegate() {
        return focusedEventId.length > 0 ? (visibleTimelineDelegates[focusedEventId] || null) : null;
    }

    function primaryActionDelegate() {
        if (primaryActionEventId.length === 0)
            return null;

        return visibleTimelineDelegates[primaryActionEventId] || null;
    }

    function primaryActionRoomModel() {
        const delegateItem = primaryActionDelegate();
        return delegateItem && delegateItem.roomModelOverride ? delegateItem.roomModelOverride : null;
    }

    function focusWalkModeEventById(eventId, options) {
        const normalizedEventId = String(eventId || "");
        if (normalizedEventId.length === 0)
            return false;

        focusedEventId = normalizedEventId;
        walkModeActive = true;
        const shouldDeferFocus = !!(options && options.deferFocus);
        if (shouldDeferFocus) {
            Qt.callLater(function () {
                if (root.walkModeActive && root.focusedEventId === normalizedEventId)
                    root.focusTimelineSelection();
            });
        } else {
            focusTimelineSelection();
        }

        const skipScroll = !!(options && options.skipScroll);
        if (!skipScroll)
            jumpToLoadedMatrixEvent(normalizedEventId);

        return true;
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

    function clearWalkState(options) {
        const shouldFocusComposer = !!(options && options.focusComposer);

        clearSelectedEvents();
        clearFocusedEvent();
        walkModeActive = false;
        suppressNextWalkModeOlderStep = false;
        walkModeEntrySuppressTimer.stop();

        if (shouldFocusComposer) {
            Qt.callLater(function () {
                root.focusTextInput();
            });
        }
    }

    function handleMouseSelectionToggle(eventId) {
        const normalizedEventId = String(eventId || "");
        if (normalizedEventId.length === 0)
            return false;

        if (!walkModeActive)
            clearWalkState({
                "focusComposer": false
            });

        if (!focusWalkModeEventById(normalizedEventId, {
                "skipScroll": true
            })) {
            return false;
        }

        const handled = toggleSelectionForEventId(normalizedEventId);
        if (!handled)
            return false;

        if (selectedEventIds.length === 0) {
            selectionAnchorEventId = "";
        }

        return handled;
    }

    function enterWalkModeFromBottomMostVisible() {
        if (!hasTimeline || hasPendingAttachments || editing)
            return false;
        if (TimelineManager.matrixTimelineReplyEventId.length > 0)
            return false;

        clearWalkState({
            "focusComposer": false
        });
        suppressNextWalkModeOlderStep = true;
        walkModeEntrySuppressTimer.restart();
        if (isEffectivelyAtLiveEdge())
            return focusLatestWalkModeEvent({
                    "deferFocus": true
                });

        const targetEventId = bottomMostVisibleEventId();
        if (targetEventId.length === 0)
            return focusLatestWalkModeEvent({
                    "deferFocus": true
                });

        return focusWalkModeEventById(targetEventId, {
                "skipScroll": true,
                "deferFocus": true
            });
    }

    function enterWalkModeAndMoveTowardOlderEventsByChunk() {
        if (!walkModeActive) {
            if (!enterWalkModeFromBottomMostVisible())
                return false;
        }

        return moveFocusTowardOlderEventsByChunk() || walkModeActive;
    }

    function lastRoomHeaderActionButtonTarget() {
        return topBar && typeof topBar.lastVisibleActionButtonItem === "function"
            ? topBar.lastVisibleActionButtonItem()
            : null;
    }

    function handleEscape() {
        if (!walkModeActive && selectedEventIds.length === 0 && !hasFocusedEvent)
            return false;

        if (selectedEventIds.length > 0) {
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
        return !!item && String(item.eventId || "").length > 0 && String(item.itemKind || "") !== "date_divider";
    }

    function focusMatrixTimelineRow(row, options) {
        if (!isSelectableMatrixTimelineRow(row))
            return false;

        const item = TimelineManager.matrixTimelineModel.itemAt(row);
        return focusWalkModeEventById(String(item.eventId || ""), options || {});
    }

    function moveFocusByStep(step) {
        const currentRow = matrixTimelineRowForEventId(focusedEventId);
        if (currentRow < 0)
            return false;

        for (let row = currentRow + step; row >= 0 && row < TimelineManager.matrixTimelineItemCount; row += step) {
            if (focusMatrixTimelineRow(row))
                return true;
        }

        return false;
    }

    function walkModeChunkSize() {
        if (!matrixTimelineList)
            return 4;

        return Math.max(4, Math.floor(Math.max(matrixTimelineList.height, 1) / 240));
    }

    function moveFocusByChunk(step) {
        const currentRow = matrixTimelineRowForEventId(focusedEventId);
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

        return messageActionSupport.openOptionsDialog(root, delegateItem, roomModel);
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
        if (exitsToComposer)
            exitWalkMode({
                "focusComposer": false
            });

        switch (actionName) {
        case "reply":
            return messageActionSupport.applyReply(roomModel, delegateItem);
        case "thread":
            return messageActionSupport.applyThread(roomModel, delegateItem);
        case "edit":
            return messageActionSupport.applyEdit(roomModel, delegateItem);
        case "forward":
            return messageActionSupport.applyForward(root, roomModel, delegateItem);
        case "remove":
            return messageActionSupport.applyRemove(root, roomModel, delegateItem);
        case "raw":
            return messageActionSupport.applyViewRaw(roomModel, delegateItem);
        case "options":
            return messageActionSupport.openOptionsDialog(root, delegateItem, roomModel);
        default:
            return false;
        }
    }

    function handleWalkModeKey(event) {
        if (!event || !walkModeActive)
            return false;

        if (event.key === Qt.Key_Escape) {
            handleEscape();
            event.accepted = true;
            return true;
        }

        if (event.key === Qt.Key_Up
                && (event.modifiers === Qt.NoModifier || event.modifiers === Qt.KeypadModifier)) {
            if (suppressNextWalkModeOlderStep) {
                suppressNextWalkModeOlderStep = false;
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
            if (focusedEventId.length > 0)
                toggleSelectionForEventId(focusedEventId);
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
        if (!walkModeActive && !hasFocusedEvent && !hasSelectedEvents)
            return false;

        clearWalkState(options);
        return true;
    }

    function timelineSelectionFocusTarget() {
        return matrixTimelineList;
    }

    function focusTimelineSelection() {
        if (!matrixTimelineList)
            return false;

        matrixTimelineList.forceActiveFocus();
        return true;
    }

    function openWalkModeHelpDialog() {
        if (root.chatRoot && typeof root.chatRoot.openWalkModeHelpDialog === "function")
            return root.chatRoot.openWalkModeHelpDialog();

        return false;
    }

    function ensureInitialBottomPin() {
        const roomId = activeRoomId;
        if (!matrixTimelineList
                || roomId.length === 0
                || loading
                || !hasTimeline)
            return;

        // Only run during the very first load of this room.
        // Once the user has seen the timeline (visibleIndicesValid),
        // never re-pin — subsequent timeline-state changes (pagination,
        // live sync) must not override the user's scroll position.
        if (matrixTimelineList.visibleIndicesValid)
            return;

        initialBottomPinPending = true;
        matrixTimelineList.keepPinnedToBottom = true;
        matrixTimelineList.maybeScrollToBottom(true);

        Qt.callLater(function () {
            if (!matrixTimelineList
                    || root.activeRoomId !== roomId
                    || root.loading
                    || !root.hasTimeline)
                return;

            matrixTimelineList.forceLayout();
            if (matrixTimelineList.keepPinnedToBottom)
                matrixTimelineList.maybeScrollToBottom(true);
            matrixTimelineList.updateLastScroll();
            if (matrixTimelineList.atYEnd)
                root.initialBottomPinPending = false;
        });
    }

    function maybeRequestInitialTimelineBuffer() {
        if (!matrixTimelineList
                || !initialTimelineBufferPending
                || initialBottomPinPending
                || bufferPaginationInFlight
                || loading
                || !hasTimeline)
            return;

        const viewportHeight = matrixTimelineList.height;
        if (viewportHeight <= 0)
            return;

        // Wait until delegates have actually rendered — contentHeight
        // is 0 before the first layout pass, and checking then would
        // trigger a premature top-up request.
        if (matrixTimelineList.contentHeight <= 0)
            return;

        if (!perfLoggedContentHeightReady) {
            perfLoggedContentHeightReady = true;
            root.markRoomSwitchPerfPhase("qml.matrix_room.content_height_ready");
        }

        const usefulBufferedHeight = viewportHeight * 0.8;
        const desiredBufferedHeight = viewportHeight + Math.min(viewportHeight * 0.25, 320);
        if (matrixTimelineList.contentHeight >= usefulBufferedHeight) {
            if (!perfLoggedUsefulHeightReady) {
                perfLoggedUsefulHeightReady = true;
                root.markRoomSwitchPerfPhase("qml.matrix_room.useful_height_ready");
            }

            if (matrixTimelineList.contentHeight >= desiredBufferedHeight) {
                if (!perfLoggedBufferFilled) {
                    perfLoggedBufferFilled = true;
                    root.markRoomSwitchPerfPhase("qml.matrix_room.buffer_filled");
                }
                console.info("[timeline-load] Buffer filled: contentH="
                    + Math.round(matrixTimelineList.contentHeight)
                    + " desired=" + Math.round(desiredBufferedHeight)
                    + " count=" + TimelineManager.matrixTimelineItemCount);
                initialTimelineBufferPending = false;
                deferredInitialBufferTopUpPending = false;
                bufferPaginationInFlight = false;
                lastInitialBufferTriggerCount = -1;
                deferredBufferCheckTimer.stop();
                return;
            }

            // Once most of the viewport is covered, let the room become
            // interactive and continue the final backfill one beat later.
            initialTimelineBufferPending = false;
            deferredInitialBufferTopUpPending = true;
            deferredBufferCheckTimer.restart();
            return;
        }

        // Previous pagination delivered items but the buffer is still
        // not full — clear the in-flight flag so we can request more.
        // Only clear when contentHeight is valid (> 0) to avoid
        // clearing during transient layout states.
        bufferPaginationInFlight = false;

        const itemCount = TimelineManager.matrixTimelineItemCount;
        if (itemCount <= 0 || lastInitialBufferTriggerCount === itemCount)
            return;

        console.info("[timeline-load] Requesting buffer top-up: contentH="
            + Math.round(matrixTimelineList.contentHeight)
            + " desired=" + Math.round(desiredBufferedHeight)
            + " count=" + itemCount
            + " requesting=6");
        if (!TimelineManager.paginateActiveMatrixTimelineBackwards(6)) {
            initialTimelineBufferPending = false;
            bufferPaginationInFlight = false;
            lastInitialBufferTriggerCount = -1;
            return;
        }

        bufferPaginationInFlight = true;
        lastInitialBufferTriggerCount = itemCount;
    }

    function maybeRequestDeferredInitialTimelineBuffer() {
        if (!matrixTimelineList
                || !deferredInitialBufferTopUpPending
                || initialBottomPinPending
                || bufferPaginationInFlight
                || loading
                || !hasTimeline) {
            return;
        }

        const viewportHeight = matrixTimelineList.height;
        if (viewportHeight <= 0 || matrixTimelineList.contentHeight <= 0)
            return;

        const desiredBufferedHeight = viewportHeight + Math.min(viewportHeight * 0.25, 320);
        if (matrixTimelineList.contentHeight >= desiredBufferedHeight) {
            if (!perfLoggedBufferFilled) {
                perfLoggedBufferFilled = true;
                root.markRoomSwitchPerfPhase("qml.matrix_room.buffer_filled");
            }
            console.info("[timeline-load] Buffer filled: contentH="
                + Math.round(matrixTimelineList.contentHeight)
                + " desired=" + Math.round(desiredBufferedHeight)
                + " count=" + TimelineManager.matrixTimelineItemCount);
            deferredInitialBufferTopUpPending = false;
            bufferPaginationInFlight = false;
            lastInitialBufferTriggerCount = -1;
            return;
        }

        bufferPaginationInFlight = false;

        const itemCount = TimelineManager.matrixTimelineItemCount;
        if (itemCount <= 0 || lastInitialBufferTriggerCount === itemCount)
            return;

        console.info("[timeline-load] Requesting deferred buffer top-up: contentH="
            + Math.round(matrixTimelineList.contentHeight)
            + " desired=" + Math.round(desiredBufferedHeight)
            + " count=" + itemCount
            + " requesting=6");
        if (!TimelineManager.paginateActiveMatrixTimelineBackwards(6)) {
            deferredInitialBufferTopUpPending = false;
            bufferPaginationInFlight = false;
            lastInitialBufferTriggerCount = -1;
            return;
        }

        bufferPaginationInFlight = true;
        lastInitialBufferTriggerCount = itemCount;
    }

    function matrixTimelineHeightCacheKey(eventId, itemId) {
        const stableEventId = String(eventId || "").trim();
        if (stableEventId.length > 0)
            return stableEventId;
        return String(itemId || "").trim();
    }

    function rememberedTimelineHeight(cacheKey) {
        if (!cacheKey || measuredTimelineHeights[cacheKey] === undefined)
            return 0;
        return Number(measuredTimelineHeights[cacheKey] || 0);
    }

    function rememberTimelineHeight(cacheKey, height) {
        const stableKey = String(cacheKey || "").trim();
        const stableHeight = Math.round(Number(height || 0));
        if (stableKey.length === 0 || stableHeight <= 0)
            return;
        if (Number(measuredTimelineHeights[stableKey] || 0) === stableHeight)
            return;

        measuredTimelineHeights[stableKey] = stableHeight;
        measuredTimelineHeightsChanged();
    }

    function matrixEventTypeForItemKind(kind) {
        switch (kind) {
        case "notice":
            return MtxEvent.NoticeMessage;
        case "redacted":
            return MtxEvent.Redacted;
        case "image":
            return MtxEvent.ImageMessage;
        case "video":
            return MtxEvent.VideoMessage;
        case "audio":
            return MtxEvent.AudioMessage;
        case "file":
            return MtxEvent.FileMessage;
        case "sticker":
            return MtxEvent.Sticker;
        default:
            return MtxEvent.TextMessage;
        }
    }

    function matrixTimelineDayKey(timestampMs) {
        const day = new Date(Number(timestampMs || 0));
        return day.getFullYear() * 10000 + (day.getMonth() + 1) * 100 + day.getDate();
    }

    function isMatrixStateLikeKind(kind) {
        return ["membership_change", "profile_change", "other_state", "failed_to_parse_state", "date_divider"].indexOf(String(kind || "")) >= 0;
    }

    function formattedMatrixTextHtml(text) {
        return TimelineManager.formatMatrixMessageHtml(String(text || ""));
    }

    function matrixStateEventIconForKind(kind) {
        switch (String(kind || "")) {
        case "membership_change":
            return ":/icons/icons/ui/state-member-join.svg";
        case "profile_change":
            return ":/icons/icons/ui/state-member-display-name.svg";
        default:
            return ":/icons/icons/ui/state-event.svg";
        }
    }

    function matrixRedactedEventPair(senderDisplayName, senderId) {
        const senderLabel = String(senderDisplayName || senderId || "").trim();
        if (senderLabel.length === 0) {
            return {
                "first": qsTr("Deleted message"),
                "second": ""
            };
        }

        return {
            "first": qsTr("Deleted message"),
            "second": qsTr("Originally sent by %1").arg(senderLabel)
        };
    }

    function openMatrixMessageContextMenu(messageModel, roomModel, copyText) {
        if (!messageModel || !roomModel || !messageModel.eventId)
            return;

        matrixMessageContextMenu.show(messageModel.eventId,
                                      messageModel.threadId || "",
                                      messageModel.type,
                                      !!messageModel.isSender,
                                      !!messageModel.isEncrypted,
                                      !!messageModel.isEditable,
                                      !!messageModel.isStateEvent,
                                      "",
                                      copyText || "",
                                      null,
                                      messageModel,
                                      roomModel);
    }

    function jumpToLoadedMatrixEvent(eventId) {
        const trimmedEventId = String(eventId || "").trim();
        if (trimmedEventId.length === 0 || !TimelineManager.matrixTimelineModel || !matrixTimelineList)
            return false;

        const row = TimelineManager.matrixTimelineModel.rowForEventId(trimmedEventId);
        if (row < 0)
            return false;

        matrixTimelineList.positionViewAtIndex(row, ListView.Center);
        return true;
    }

    function focusTextInput() {
        return composerInput ? composerInput.focusTextInput() : false;
    }

    function destroyOnClose(dialog) {
        if (!dialog)
            return;

        if (root.chatRoot && root.chatRoot.dialogHost && root.chatRoot.dialogHost.destroyOnClose != undefined) {
            root.chatRoot.dialogHost.destroyOnClose(dialog);
            return;
        }

        if (dialog.closing != undefined)
            dialog.closing.connect(() => dialog.destroy(1000));
        else if (dialog.aboutToHide != undefined)
            dialog.aboutToHide.connect(() => dialog.destroy(1000));
    }

    function showDialogFromComponent(componentRef, properties) {
        const dialogParent = root.chatRoot && root.chatRoot.dialogHost
            ? root.chatRoot.dialogHost
            : (root.chatRoot ? root.chatRoot : root);
        const dialog = componentRef.createObject(dialogParent, properties || {});
        if (!dialog)
            return null;
        dialog.open();
        root.destroyOnClose(dialog);
        return dialog;
    }

    function appendText(text) {
        return composerInput ? composerInput.appendText(text) : false;
    }

    function trySendMessage() {
        if (root.hasPendingAttachments)
            return TimelineManager.sendActiveMatrixAttachments();

        const body = composerInput.text;
        const ok = root.editing
            ? TimelineManager.sendActiveMatrixEditMessage(body)
            : TimelineManager.sendActiveMatrixTextMessage(body);
        if (!ok)
            return false;

        if (!root.editing) {
            composerInput.replaceText("");
            matrixComposerInputController.setText("");
        }
        root.focusTextInput();
        return true;
    }

    function beginEdit(eventId, body, messageKind) {
        if (!eventId || !body)
            return false;

        if (!root.editing) {
            draftBeforeEdit = composerInput.text;
            restoringEditDraft = true;
        }

        if (!TimelineManager.queueActiveMatrixEdit(String(eventId), String(body), String(messageKind || "message"))) {
            if (restoringEditDraft) {
                draftBeforeEdit = "";
                restoringEditDraft = false;
            }
            return false;
        }

        matrixComposerInputController.setText(String(body));
        root.focusTextInput();
        return true;
    }

    function openRemoveMessageDialog(eventId) {
        return roomSupport.openRemoveMessageDialog(eventId);
    }

    function openRawMessageDialog(eventId) {
        return roomSupport.openRawMessageDialog(eventId);
    }

    function openReadReceiptsDialog(eventId) {
        return roomSupport.openReadReceiptsDialog(eventId);
    }

    function openMatrixForwardDialog(eventId) {
        return roomSupport.openMatrixForwardDialog(eventId);
    }

    function openForwardDialog(eventId) {
        return roomSupport.openMatrixForwardDialog(eventId);
    }

    function openReportMessageDialog(eventId) {
        return roomSupport.openReportMessageDialog(eventId);
    }

    function openMessageActionsDialog(eventId,
                                      threadId,
                                      eventType,
                                      isSender,
                                      isEncrypted,
                                      isEditable,
                                      link,
                                      text,
                                      messageModelOverride,
                                      roomModelOverride) {
        return roomSupport.openMessageActionsDialog(eventId,
                                                    threadId,
                                                    eventType,
                                                    isSender,
                                                    isEncrypted,
                                                    isEditable,
                                                    link,
                                                    text,
                                                    messageModelOverride,
                                                    roomModelOverride);
    }

    anchors.fill: parent
    enabled: visible
    spacing: 0
    visible: !!roomPreview && roomPreview.isMatrixSummary

    RoomHeader {
        Layout.fillWidth: true
        room: null
        roomModel: matrixHeaderRoomModel
        roomId: root.roomPreview ? root.roomPreview.roomid : ""
        roomName: root.roomPreview ? root.roomPreview.roomName : qsTr("No room selected")
        avatarDisplayName: root.roomPreview ? root.roomPreview.roomName : qsTr("No room selected")
        avatarUrl: root.roomPreview ? root.roomPreview.roomAvatarUrl : ""
        directChatOtherUserId: root.roomPreview ? root.roomPreview.directChatOtherUserId : ""
        isDirect: !!root.roomPreview && root.roomPreview.isDirect
        isEncrypted: !!root.roomPreview && root.roomPreview.isEncrypted
        roomTopic: root.roomPreview ? root.roomPreview.roomTopic : ""
        showBackButton: root.showBackButton
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.preferredHeight: 1
        color: palette.mid
    }

    Rectangle {
        Layout.fillHeight: true
        Layout.fillWidth: true
        color: palette.base

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            Item {
                id: timelineViewport

                Layout.fillHeight: true
                Layout.fillWidth: true

                ScrollBar {
                    id: matrixTimelineScrollbar

                    readonly property int scrollbarPolicy: Settings.uiScrollbarPolicy
                    readonly property bool scrollbarVisible: {
                        switch (scrollbarPolicy) {
                        case Settings.ScrollbarPolicy.Always:
                            return true;
                        case Settings.ScrollbarPolicy.Never:
                            return false;
                        case Settings.ScrollbarPolicy.WhenNeeded:
                        default:
                            return matrixTimelineList.contentHeight > matrixTimelineList.height;
                        }
                    }

                    anchors.bottom: parent.bottom
                    anchors.right: parent.right
                    anchors.top: parent.top
                    parent: matrixTimelineList.parent
                    policy: scrollbarVisible ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
                    orientation: Qt.Vertical
                    // Thumb size from stabilized virtual height.
                    // Index-based thumb: size and position derived from
                    // model count and visible indices, not contentHeight.
                    // This makes the thumb completely immune to Qt's
                    // internal contentHeight fluctuations.
                    // Show full thumb until the index-based visible count
                    // has been reliably established.  After that, size =
                    // visible / total and only ever shrinks as pagination
                    // adds items.
                    size: matrixTimelineList.stableThumbSize
                    // Saved position at press time.  When pressed becomes
                    // true the Binding deactivates and Qt resets position
                    // to 0.  The drag handler uses this to reject that
                    // bogus first change.
                    property real positionOnPress: -1
                    onPressedChanged: {
                        if (pressed)
                            positionOnPress = position;
                        else
                            positionOnPress = -1;
                    }
                }

                // Position: driven by real contentY (always accurate).
                Binding {
                    target: matrixTimelineScrollbar
                    property: "position"
                    value: {
                        const ch = matrixTimelineList.contentHeight;
                        const h = matrixTimelineList.height;
                        const range = ch - h;
                        if (range <= 0)
                            return 0;
                        const oy = matrixTimelineList.originY;
                        const normalized = (matrixTimelineList.contentY - oy) / range;
                        const maxPos = 1.0 - matrixTimelineScrollbar.size;
                        return Math.max(0, Math.min(maxPos, normalized * maxPos));
                    }
                    when: !matrixTimelineScrollbar.pressed
                }

                // Drag: map scrollbar position back to contentY.
                Connections {
                    target: matrixTimelineScrollbar
                    function onPositionChanged() {
                        if (!matrixTimelineScrollbar.pressed)
                            return;
                        // When the Binding deactivates on press, Qt resets
                        // position to 0.  Detect this bogus jump and restore
                        // the saved position instead of scrolling to the top.
                        if (matrixTimelineScrollbar.positionOnPress >= 0) {
                            const saved = matrixTimelineScrollbar.positionOnPress;
                            matrixTimelineScrollbar.positionOnPress = -1;
                            if (Math.abs(matrixTimelineScrollbar.position - saved) > 0.01) {
                                matrixTimelineScrollbar.position = saved;
                                return;
                            }
                        }
                        const ch = matrixTimelineList.contentHeight;
                        const h = matrixTimelineList.height;
                        const range = ch - h;
                        if (range <= 0)
                            return;
                        const maxPos = 1.0 - matrixTimelineScrollbar.size;
                        if (maxPos <= 0)
                            return;
                        const normalized = matrixTimelineScrollbar.position / maxPos;
                        matrixTimelineList.contentY = matrixTimelineList.originY + normalized * range;
                        matrixTimelineList.returnToBounds();
                        matrixTimelineList.updateLastScroll();
                    }
                }

                TimelineToEndButton {
                    chatList: matrixTimelineList
                    scrollbarItem: matrixTimelineScrollbar
                    z: 20
                }

                ListView {
                    id: matrixTimelineList

                    property int delegateMaxWidth: width - (matrixTimelineScrollbar.interactive ? matrixTimelineScrollbar.width : 0)
                    property bool keepPinnedToBottom: true
                    // True after the user explicitly scrolls away from the
                    // bottom.  Prevents layout-driven contentY adjustments
                    // (BottomToTop can shift contentY toward 0 when
                    // contentHeight shrinks) from re-enabling bottom pin.
                    // Cleared only by onMovementEnded at atYEnd (deliberate
                    // return to bottom).
                    property bool userUnpinned: false
                    // Last known top-visible index, saved during wheel
                    // scroll when indexAt returns valid values.  Used to
                    // restore position after Qt-internal model resets.
                    property int savedTopIndex: -1
                    property int previousCount: 0
                    property real lastScrollPos: 0

                    // Index-based scrollbar state.  Updated via
                    // Stable thumb size — updated only at rest points and
                    // constrained to never grow within a room session.
                    // This makes the thumb immune to mid-scroll
                    // contentHeight fluctuations.
                    property real stableThumbSize: 1.0
                    property bool visibleIndicesValid: false

                    function updateStableThumbSize() {
                        if (count <= 0 || height <= 0 || contentHeight <= height)
                            return;
                        const newSize = Math.max(0.02, height / contentHeight);
                        // Only allow the thumb to shrink, never grow.
                        // contentHeight can fluctuate upward transiently
                        // (bad estimates), but it settles downward to the
                        // correct total.  "Only shrink" ensures transient
                        // over-estimates don't cause visible thumb growth.
                        if (!visibleIndicesValid || newSize < stableThumbSize) {
                            stableThumbSize = newSize;
                            visibleIndicesValid = true;
                        }
                    }

                    function updateLastScroll() {
                        lastScrollPos = contentY + height;
                    }





                    function updateBottomPin() {
                        if (root.initialBottomPinPending) {
                            keepPinnedToBottom = true;
                            if (atYEnd) {
                                root.initialBottomPinPending = false;
                                bufferCheckTimer.restart();

                            }
                            return;
                        }

                        keepPinnedToBottom = atYEnd;
                    }

                    function maybeScrollToBottom(force) {
                        if (count <= 0 || userUnpinned)
                            return;

                        if (!(force || keepPinnedToBottom || root.initialBottomPinPending))
                            return;

                        Qt.callLater(function () {
                            if (count <= 0 || userUnpinned
                                    || !(force || keepPinnedToBottom || root.initialBottomPinPending))
                                return;

                            positionViewAtBeginning();
                            updateBottomPin();
                        });
                    }

                    anchors.fill: parent
                    anchors.margins: Komai.paddingLarge
                    anchors.rightMargin: Komai.paddingLarge + (matrixTimelineScrollbar.interactive ? matrixTimelineScrollbar.width : 0)
                    keyNavigationEnabled: false
                    KeyNavigation.priority: KeyNavigation.BeforeItem
                    Keys.priority: Keys.BeforeItem
                    clip: true
                    reuseItems: true
                    // Index 0 = newest (model is reversed in Rust).
                    // BottomToTop places index 0 at the visual bottom, matching
                    // chat convention (newest at bottom). This also makes
                    // contentHeight changes extend upward, away from the anchored
                    // viewport, which keeps the scrollbar thumb stable.
                    verticalLayoutDirection: ListView.BottomToTop
                    boundsBehavior: Flickable.StopAtBounds
                    displayMarginBeginning: root.chatRoot && root.chatRoot.listViewDisplayMargin !== undefined
                        ? root.chatRoot.listViewDisplayMargin
                        : 0
                    displayMarginEnd: root.chatRoot && root.chatRoot.listViewDisplayMargin !== undefined
                        ? root.chatRoot.listViewDisplayMargin
                        : 0
                    cacheBuffer: {
                        const baseBuffer = root.chatRoot && root.chatRoot.listViewCacheBuffer !== undefined
                            ? root.chatRoot.listViewCacheBuffer
                            : 320;
                        if (root.chatRoot && root.chatRoot.roomSwitchInProgress)
                            return 0;
                        return baseBuffer;
                    }
                    model: TimelineManager.matrixTimelineModel
                    spacing: Komai.paddingMedium
                    visible: root.hasTimeline

                    Keys.onPressed: event => {
                        root.handleWalkModeKey(event);
                    }
                    Keys.onShortcutOverride: event => {
                        if (event.key === Qt.Key_Escape
                                && (root.walkModeActive || root.hasSelectedEvents || root.hasFocusedEvent)) {
                            event.accepted = true;
                        }
                    }

                    // Settle timer: re-evaluate keepPinnedToBottom after
                    // the user stops wheel-scrolling for 250 ms.  During
                    // active scrolling, keepPinnedToBottom is only ever
                    // CLEARED (never set true) to avoid transient atYEnd
                    // states from contentHeight fluctuations.
                    Timer {
                        id: wheelSettleTimer
                        interval: 250
                        onTriggered: {
                            // Only re-pin if the user hasn't explicitly
                            // scrolled away.  Layout adjustments can move
                            // contentY toward the bottom, but that's not
                            // the user's intent.
                            if (!matrixTimelineList.userUnpinned)
                                matrixTimelineList.keepPinnedToBottom = matrixTimelineList.atYEnd;
                            matrixTimelineList.updateStableThumbSize();
                        }
                    }

                    WheelHandler {
                        orientation: Qt.Vertical
                        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad

                        property real previousRotation: 0

                        onRotationChanged: {
                            const delta = rotation - previousRotation;
                            previousRotation = rotation;
                            matrixTimelineList.contentY -= delta * 5;
                            matrixTimelineList.returnToBounds();
                            matrixTimelineList.updateLastScroll();
                            // Save top visible index for model-reset recovery
                            const halfW = Math.max(1, Math.round(matrixTimelineList.width / 2));
                            const idx = matrixTimelineList.indexAt(halfW, matrixTimelineList.contentY + 2);
                            if (idx >= 0)
                                matrixTimelineList.savedTopIndex = idx;
                            if (!matrixTimelineList.atYEnd) {
                                matrixTimelineList.keepPinnedToBottom = false;
                                matrixTimelineList.userUnpinned = true;
                                if (root.initialBottomPinPending)
                                    root.initialBottomPinPending = false;
                                if (root.initialTimelineBufferPending)
                                    root.initialTimelineBufferPending = false;
                                if (root.deferredInitialBufferTopUpPending)
                                    root.deferredInitialBufferTopUpPending = false;
                                deferredBufferCheckTimer.stop();
                            }
                            wheelSettleTimer.restart();
                        }
                    }

                    onMovementEnded: {
                        updateLastScroll();
                        keepPinnedToBottom = atYEnd;
                        if (atYEnd)
                            userUnpinned = false;
                        root.scheduleReadMarkerUpdate(atYEnd);
                        updateStableThumbSize();
                    }
                    onAtYBeginningChanged: {
                        // Don't trigger scroll-based pagination while the
                        // initial buffer is still filling — atYBeginning
                        // is transiently true during initial load because
                        // content is shorter than the viewport.
                        if (atYBeginning
                                && root.hasTimeline
                                && !root.loading
                                && !root.initialTimelineBufferPending
                                && root.lastPaginationTriggerCount !== TimelineManager.matrixTimelineItemCount) {
                            console.info("[timeline-load] Scroll-triggered pagination at top, count="
                                + TimelineManager.matrixTimelineItemCount);
                            if (TimelineManager.paginateActiveMatrixTimelineBackwards(0))
                                root.lastPaginationTriggerCount = TimelineManager.matrixTimelineItemCount;
                        }
                    }
                    onContentYChanged: {
                        // Reset the pagination latch once the user scrolls away
                        // from the top edge.  In BottomToTop contentY is usually
                        // negative; use !atYBeginning as the reliable check.
                        if (!atYBeginning && root.lastPaginationTriggerCount === TimelineManager.matrixTimelineItemCount)
                            root.lastPaginationTriggerCount = -1;

                        // Cancel initial-pin/buffer if user actively scrolled
                        // away from the bottom during touch/drag/flick.
                        if ((moving || flicking || dragging) && !atYEnd) {
                            if (root.initialBottomPinPending)
                                root.initialBottomPinPending = false;
                            if (root.initialTimelineBufferPending)
                                root.initialTimelineBufferPending = false;
                            if (root.deferredInitialBufferTopUpPending)
                                root.deferredInitialBufferTopUpPending = false;
                            deferredBufferCheckTimer.stop();
                        }

                        // Do NOT call updateBottomPin() here.  Transient
                        // contentHeight fluctuations can briefly make atYEnd
                        // true, which would set keepPinnedToBottom = true and
                        // cause auto-scroll on the next content update.
                        // keepPinnedToBottom is updated only from explicit
                        // user actions (onMovementEnded, WheelHandler).
                    }
                    onContentHeightChanged: {
                        // While the user is holding the scrollbar thumb,
                        // the position Binding is inactive.  Recompute
                        // position from contentY so the thumb reflects
                        // the new proportions after pagination adds items.
                        if (matrixTimelineScrollbar.pressed) {
                            const range = contentHeight - height;
                            if (range > 0) {
                                const normalized = (contentY - originY) / range;
                                const maxPos = 1.0 - matrixTimelineScrollbar.size;
                                matrixTimelineScrollbar.position = Math.max(0,
                                    Math.min(maxPos, normalized * maxPos));
                            }
                        }

                        if (!moving && !flicking && !dragging && !userUnpinned) {
                            if (keepPinnedToBottom || root.initialBottomPinPending) {
                                positionViewAtBeginning();
                                updateBottomPin();
                            } else {
                                maybeScrollToBottom(previousCount === 0);
                            }
                            updateLastScroll();
                        }
                        if (root.deferredInitialBufferTopUpPending)
                            deferredBufferCheckTimer.restart();
                        else
                            bufferCheckTimer.restart();
                    }
                    onHeightChanged: {
                        if (!moving && !flicking && !dragging && !userUnpinned) {
                            if (keepPinnedToBottom || root.initialBottomPinPending) {
                                positionViewAtBeginning();
                                updateBottomPin();
                            } else {
                                contentY = lastScrollPos - height;
                                maybeScrollToBottom(previousCount === 0);
                            }
                            updateLastScroll();
                        }
                        if (root.deferredInitialBufferTopUpPending)
                            deferredBufferCheckTimer.restart();
                        else
                            bufferCheckTimer.restart();
                    }
                    onCountChanged: {
                        // Pagination delivered new items — allow buffer
                        // check to re-evaluate on the next trigger.
                        if (count !== previousCount)
                            root.bufferPaginationInFlight = false;
                        if (count > 0 && !root.perfLoggedCountNonZero) {
                            root.perfLoggedCountNonZero = true;
                            root.markRoomSwitchPerfPhase("qml.matrix_room.count_nonzero");
                        }
                        const forceScroll = previousCount === 0 && !visibleIndicesValid;
                        if (!userUnpinned && (forceScroll || keepPinnedToBottom || root.initialBottomPinPending)) {
                            positionViewAtBeginning();
                            updateBottomPin();
                        } else {
                            maybeScrollToBottom(forceScroll);
                        }
                        updateLastScroll();
                        Qt.callLater(updateStableThumbSize);
                        if (root.deferredInitialBufferTopUpPending)
                            deferredBufferCheckTimer.restart();
                        else
                            bufferCheckTimer.restart();
                        root.scheduleReadMarkerUpdate(!userUnpinned
                            && (forceScroll || keepPinnedToBottom || root.initialBottomPinPending || atYEnd));
                        previousCount = count;
                    }
                    onModelChanged: {
                        previousCount = count;
                        if (!userUnpinned && keepPinnedToBottom && count > 0)
                            positionViewAtBeginning();
                        updateLastScroll();
                    }
                    Component.onCompleted: {
                        previousCount = count;
                        updateLastScroll();
                        maybeScrollToBottom(true);
                    }







                    delegate: Item {
                        id: timelineItemDelegate

                        property var chat: matrixTimelineList
                        property var chatRoot: root

                        required property string itemKind
                        required property string itemId
                        required property string eventId
                        required property string deliveryState
                        required property string threadId
                        required property string senderDisplayName
                        required property string senderAvatarUrl
                        required property string senderId
                        required property string body
                        required property string replyEventId
                        required property string replySenderId
                        required property string replySenderDisplayName
                        required property string replyBody
                        required property var reactions
                        required property string reactionsSummary
                        required property string fileName
                        required property string mimeType
                        required property string mediaUrl
                        required property string thumbnailUrl
                        required property double mediaWidth
                        required property double mediaHeight
                        required property double mediaDurationMs
                        required property double mediaSizeBytes
                        required property bool mediaIsEncrypted
                        required property bool thumbnailIsEncrypted
                        required property double timestamp
                        required property bool isEdited
                        required property bool isOwn

                        readonly property int modelIndex: TimelineManager.matrixTimelineModel
                            ? TimelineManager.matrixTimelineModel.rowForEventId(eventId)
                            : -1
                        readonly property bool isMediaItem: ["image", "video", "audio", "file", "sticker"].indexOf(itemKind) >= 0
                        readonly property string effectiveFileName: fileName.length > 0 ? fileName : (body.length > 0 ? body : qsTr("Attachment"))
                        readonly property string replySourceBody: body.length > 0 ? body : effectiveFileName
                        readonly property double safePreviewAspectRatio: mediaWidth > 0 && mediaHeight > 0 ? (mediaHeight / mediaWidth) : 0.75
                        readonly property bool isStateLikeItem: ["membership_change", "profile_change", "other_state", "failed_to_parse_state"].indexOf(itemKind) >= 0
                        readonly property bool usesSharedImageBubble: itemKind === "image"
                        readonly property bool usesSharedStickerBubble: itemKind === "sticker"
                        readonly property bool usesSharedVideoBubble: itemKind === "video"
                        readonly property bool usesSharedFileBubble: itemKind === "file"
                        readonly property bool usesSharedAudioBubble: itemKind === "audio"
                        readonly property bool usesSharedStateBubble: isStateLikeItem
                        readonly property bool usesSharedTextBubble: itemKind !== "date_divider"
                            && !isStateLikeItem
                            && !isMediaItem
                        readonly property string stableMediaEventId: eventId.length > 0 ? eventId : itemId
                        readonly property bool usesSharedTimelineBubble: usesSharedTextBubble
                            || usesSharedImageBubble
                            || usesSharedStickerBubble
                            || usesSharedVideoBubble
                            || usesSharedFileBubble
                            || usesSharedAudioBubble
                            || usesSharedStateBubble
                        property bool sharedBubbleReloadArmed: false
                        readonly property string heightCacheKey: root.matrixTimelineHeightCacheKey(eventId, itemId)
                        readonly property real cachedMeasuredHeight: root.rememberedTimelineHeight(heightCacheKey)
                        readonly property bool supportsSharedToolbarActions: eventId.length > 0
                            && itemKind !== "date_divider"
                            && itemKind !== "redacted"
                            && !isStateLikeItem
                        readonly property int sharedDeliveryStatus: deliveryState === "sent"
                            ? MtxEvent.Sent
                            : deliveryState === "failed"
                            ? MtxEvent.Failed
                            : deliveryState === "read"
                            ? MtxEvent.Read
                            : deliveryState === "received"
                            ? MtxEvent.Received
                            : MtxEvent.Empty
                        readonly property int matrixEventType: root.matrixEventTypeForItemKind(itemKind)
                        readonly property int dayKey: root.matrixTimelineDayKey(timestamp)
                        readonly property var previousItem: TimelineManager.matrixTimelineModel && modelIndex > 0
                            ? TimelineManager.matrixTimelineModel.itemAt(modelIndex - 1)
                            : ({})
                        readonly property string sharedHumanReadableMediaSize: mediaSizeBytes > 0
                            ? Komai.humanReadableFileSize(Number(mediaSizeBytes))
                            : ""
                        readonly property string sharedFileTypeIconSource: Komai.fileTypeIconSource(mimeType)
                        readonly property var sharedRedactedPair: root.matrixRedactedEventPair(senderDisplayName,
                                                                                               senderId)
                        readonly property var sharedPreviewData: ({
                                "room": matrixToolbarRoomModel,
                                "avatarUrl": senderAvatarUrl,
                                "body": body,
                                "formattedBody": root.formattedMatrixTextHtml(body),
                                "isOnlyEmoji": 0,
                                "redactedFirst": sharedRedactedPair.first,
                                "redactedSecond": sharedRedactedPair.second,
                                "previousDay": previousItem.timestamp !== undefined ? root.matrixTimelineDayKey(previousItem.timestamp) : dayKey,
                                "previousTimestamp": previousItem.timestamp !== undefined ? new Date(Number(previousItem.timestamp)) : new Date(Number(timestamp)),
                                "previousIsStateEvent": previousItem.eventId === undefined ? true : root.isMatrixStateLikeKind(previousItem.itemKind),
                                "previousUserId": previousItem.senderId !== undefined ? String(previousItem.senderId || "") : ""
                            })
                        readonly property var sharedAttachmentPreviewData: ({
                                "room": matrixToolbarRoomModel,
                                "avatarUrl": senderAvatarUrl,
                                "eventId": stableMediaEventId,
                                "body": body,
                                "filename": effectiveFileName,
                                "filesize": sharedHumanReadableMediaSize,
                                "fileTypeIconSource": sharedFileTypeIconSource,
                                "mimetype": mimeType,
                                "duration": Math.round(Number(mediaDurationMs)),
                                "previousDay": previousItem.timestamp !== undefined ? root.matrixTimelineDayKey(previousItem.timestamp) : dayKey,
                                "previousTimestamp": previousItem.timestamp !== undefined ? new Date(Number(previousItem.timestamp)) : new Date(Number(timestamp)),
                                "previousIsStateEvent": previousItem.eventId === undefined ? true : root.isMatrixStateLikeKind(previousItem.itemKind),
                                "previousUserId": previousItem.senderId !== undefined ? String(previousItem.senderId || "") : ""
                            })
                        readonly property var sharedVisualPreviewData: ({
                                "room": matrixToolbarRoomModel,
                                "avatarUrl": senderAvatarUrl,
                                "url": mediaUrl,
                                "blurhash": "",
                                "eventId": stableMediaEventId,
                                "body": body,
                                "filename": effectiveFileName,
                                "filesize": sharedHumanReadableMediaSize,
                                "filesizeBytes": Math.round(Number(mediaSizeBytes)),
                                "mimetype": mimeType,
                                "thumbnailUrl": thumbnailUrl,
                                "originalWidth": Math.round(Number(mediaWidth)),
                                "originalHeight": Math.round(Number(mediaHeight)),
                                "proportionalHeight": safePreviewAspectRatio,
                                "containerHeight": matrixTimelineList.height > 0 ? matrixTimelineList.height : root.height,
                                "previousDay": previousItem.timestamp !== undefined ? root.matrixTimelineDayKey(previousItem.timestamp) : dayKey,
                                "previousTimestamp": previousItem.timestamp !== undefined ? new Date(Number(previousItem.timestamp)) : new Date(Number(timestamp)),
                                "previousIsStateEvent": previousItem.eventId === undefined ? true : root.isMatrixStateLikeKind(previousItem.itemKind),
                                "previousUserId": previousItem.senderId !== undefined ? String(previousItem.senderId || "") : ""
                            })
                        readonly property var sharedReplyPreviewData: replyEventId.length > 0
                            ? ({
                                "type": MtxEvent.TextMessage,
                                "body": replyBody,
                                "formattedBody": root.formattedMatrixTextHtml(replyBody),
                                "isOnlyEmoji": 0,
                                "userId": replySenderId,
                                "userName": replySenderDisplayName.length > 0 ? replySenderDisplayName : qsTr("Reply")
                            })
                            : ({})
                        readonly property var sharedStatePreviewData: ({
                                "room": matrixToolbarRoomModel,
                                "avatarUrl": senderAvatarUrl,
                                "formattedStateEvent": root.formattedMatrixTextHtml(body),
                                "stateEventIconSource": root.matrixStateEventIconForKind(itemKind),
                                "previousDay": previousItem.timestamp !== undefined ? root.matrixTimelineDayKey(previousItem.timestamp) : dayKey,
                                "previousTimestamp": previousItem.timestamp !== undefined ? new Date(Number(previousItem.timestamp)) : new Date(Number(timestamp)),
                                "previousIsStateEvent": previousItem.eventId === undefined ? true : root.isMatrixStateLikeKind(previousItem.itemKind),
                                "previousUserId": previousItem.senderId !== undefined ? String(previousItem.senderId || "") : ""
                            })
                        readonly property real heuristicTimelineHeightEstimate: {
                            const baseLineHeight = Math.max(18, Math.round(Settings.uiFontSizePt * 1.8));
                            const compactRowHeight = Math.max(root.composerBaselineHeight, baseLineHeight + Komai.paddingMedium * 2);
                            const detailRowHeight = Math.max(compactRowHeight, baseLineHeight * 3);

                            // Bubble vertical padding that the style adds around content.
                            const bubblePad = Komai.uiLayoutCompactMode
                                ? Komai.paddingSmall * 2
                                : Komai.paddingMedium * 2;

                            // Estimate section header height (avatar + username row)
                            // when the sender changes, a timestamp gap occurs, or the
                            // previous item was a different event category.
                            const prevUserId = previousItem.senderId !== undefined
                                ? String(previousItem.senderId || "") : "";
                            const prevTimestamp = previousItem.timestamp !== undefined
                                ? Number(previousItem.timestamp) : 0;
                            const prevDay = previousItem.timestamp !== undefined
                                ? root.matrixTimelineDayKey(previousItem.timestamp) : dayKey;
                            const prevIsState = previousItem.eventId === undefined
                                ? true : root.isMatrixStateLikeKind(previousItem.itemKind);
                            const dayChanged = prevDay !== dayKey;
                            const showSection = dayChanged
                                || (timestamp - prevTimestamp > 3600000);
                            const startsGroup = prevUserId !== senderId
                                || showSection
                                || prevIsState !== isStateLikeItem;
                            const sectionEstimate = startsGroup
                                ? (baseLineHeight + Komai.paddingMedium
                                   + (dayChanged ? baseLineHeight + Komai.paddingSmall * 2 : 0))
                                : 0;

                            // Reaction row estimate.
                            const hasReactions = reactions && (Array.isArray(reactions)
                                ? reactions.length > 0
                                : (typeof reactions === "string" && reactions.length > 0));
                            const reactionEstimate = hasReactions
                                ? baseLineHeight + Komai.paddingSmall : 0;

                            if (usesSharedImageBubble || usesSharedStickerBubble || usesSharedVideoBubble) {
                                const viewportHeight = matrixTimelineList.height > 0 ? matrixTimelineList.height : root.height;
                                const mediaAspect = safePreviewAspectRatio > 0 ? safePreviewAspectRatio : 0.75;
                                const mediaWidthHint = mediaWidth > 0 ? mediaWidth : Math.max(baseLineHeight * 18, root.composerBaselineHeight * 5);
                                const maxMediaHeight = Math.max(1, viewportHeight / 4);
                                const mediaWidthEstimate = Math.max(1, Math.round(mediaWidthHint * Math.min(maxMediaHeight / (mediaWidthHint * mediaAspect), 1)));
                                const captionEstimate = (body.length > 0 && !body.match(/\.\w{2,5}$/))
                                    ? (baseLineHeight * 2 + Komai.paddingSmall * 2)
                                    : 0;
                                return Math.max(detailRowHeight, sectionEstimate + Math.round(mediaWidthEstimate * mediaAspect) + captionEstimate + bubblePad + reactionEstimate);
                            }

                            if (usesSharedFileBubble || usesSharedAudioBubble)
                                return sectionEstimate + detailRowHeight + bubblePad + reactionEstimate;
                            if (usesSharedStateBubble)
                                return compactRowHeight;
                            if (itemKind === "redacted")
                                return sectionEstimate + compactRowHeight + reactionEstimate;

                            const estimatedLines = Math.max(1, Math.min(12, Math.ceil(String(body || "").length / 42)));
                            const replyEstimate = replyEventId.length > 0 ? detailRowHeight : 0;
                            const threadEstimate = threadId.length > 0 ? Komai.paddingSmall * 2 : 0;
                            return Math.max(compactRowHeight, sectionEstimate + bubblePad + estimatedLines * baseLineHeight + replyEstimate + threadEstimate + reactionEstimate);
                        }
                        readonly property real sharedTimelineHeightEstimate: {
                            if (itemKind === "date_divider")
                                return dateDivider.implicitHeight;
                            if (!usesSharedTimelineBubble)
                                return 0;
                            if (sharedTimelineBubble.item) {
                                // Skip the loaded style's height while it is still using
                                // the 100 px placeholder (contentReady === false).  The
                                // cache or heuristic below will be much closer to the
                                // true height, avoiding a visible snap when the body
                                // resolves one frame later.
                                const itemReady = sharedTimelineBubble.item.contentReady !== undefined
                                    ? sharedTimelineBubble.item.contentReady
                                    : true;
                                if (itemReady) {
                                    const resolvedHeight = sharedTimelineBubble.item.implicitHeight > 0
                                        ? sharedTimelineBubble.item.implicitHeight
                                        : sharedTimelineBubble.item.height;
                                    if (resolvedHeight > 0)
                                        return resolvedHeight;
                                }
                            }
                            if (cachedMeasuredHeight > 0)
                                return cachedMeasuredHeight;
                            return heuristicTimelineHeightEstimate;
                        }
                        width: matrixTimelineList.width
                        height: sharedTimelineHeightEstimate

                        function reloadSharedTimelineBubble() {
                            if (!usesSharedTimelineBubble || sharedBubbleReloadArmed)
                                return;

                            sharedBubbleReloadArmed = true;
                            Qt.callLater(function () {
                                timelineItemDelegate.sharedBubbleReloadArmed = false;
                            });
                        }

                        function rememberResolvedTimelineHeight() {
                            if (!sharedTimelineBubble.item)
                                return;

                            const resolvedHeight = sharedTimelineBubble.item.implicitHeight > 0
                                ? sharedTimelineBubble.item.implicitHeight
                                : sharedTimelineBubble.item.height;
                            if (resolvedHeight > 0)
                                root.rememberTimelineHeight(heightCacheKey, resolvedHeight);
                        }

                        onEventIdChanged: reloadSharedTimelineBubble()
                        onItemIdChanged: reloadSharedTimelineBubble()
                        onItemKindChanged: reloadSharedTimelineBubble()
                        onHeightChanged: rememberResolvedTimelineHeight()

                        PreviewPermissions {
                            id: matrixToolbarPreviewPermissions
                        }

                        QtObject {
                            id: matrixToolbarInput

                            function reaction(targetEventId, reactionKey) {
                                TimelineManager.toggleActiveMatrixTimelineReaction(
                                    String(targetEventId || timelineItemDelegate.eventId || ""),
                                    String(reactionKey || ""));
                            }
                        }

                        QtObject {
                            id: matrixToolbarRoomModel

                            property string roomId: root.roomPreview ? root.roomPreview.roomid : ""
                            property bool isActiveMatrixTimelineRoom: true
                            property int roomMemberCount: root.roomPreview && root.roomPreview.roomMemberCount !== undefined
                                ? Number(root.roomPreview.roomMemberCount)
                                : 0
                            property bool isEncrypted: root.roomPreview ? !!root.roomPreview.isEncrypted : false
                            property AbstractPermissions permissions: matrixToolbarPreviewPermissions
                            property var input: matrixToolbarInput
                            property var frequentReactions: []
                            property var pinnedMessages: TimelineManager.matrixTimelinePinnedEventIds
                            property string reply: ""
                            property string edit: ""
                            property string thread: ""
                            property bool supportsThreadNavigation: false

                            function formatDateSeparator(timestamp) {
                                return Qt.formatDate(timestamp, "ddd, MMM d");
                            }

                            function formatLaterSeparator(_previous, currentTimestamp) {
                                return Qt.formatTime(currentTimestamp, "hh:mm");
                            }

                            function openUserProfile(userId) {
                                matrixDialogRoomModel.openUserProfile(userId);
                            }

                            function previewDataForEvent(eventId) {
                                const preview = matrixHeaderRoomModel.previewDataForEvent(eventId);
                                return Object.assign({}, preview || {}, {
                                    "room": matrixToolbarRoomModel
                                });
                            }

                            function eventShown() {
                            }

                            function openMedia(targetEventId) {
                                const targetItemId = String(targetEventId || timelineItemDelegate.itemId || "");
                                if (targetItemId.length === 0)
                                    return;

                                TimelineManager.openActiveMatrixTimelineMedia(
                                    targetItemId,
                                    timelineItemDelegate.effectiveFileName);
                            }

                            function saveMedia(targetEventId) {
                                const targetItemId = String(targetEventId || timelineItemDelegate.itemId || "");
                                if (targetItemId.length === 0)
                                    return;

                                TimelineManager.saveActiveMatrixTimelineMedia(
                                    targetItemId,
                                    timelineItemDelegate.effectiveFileName);
                            }

                            function showImage() {
                                return true;
                            }

                            onReplyChanged: {
                                if (!reply)
                                    return;

                                TimelineManager.queueActiveMatrixReply(
                                    reply,
                                    timelineItemDelegate.senderId,
                                    timelineItemDelegate.senderDisplayName,
                                    timelineItemDelegate.replySourceBody);
                                reply = "";
                            }
                            onEditChanged: {
                                if (!edit)
                                    return;

                                root.beginEdit(edit,
                                               timelineItemDelegate.body,
                                               timelineItemDelegate.itemKind);
                                edit = "";
                            }
                            onThreadChanged: {
                                if (thread)
                                    thread = "";
                            }

                            function showEvent(eventId) {
                                return root.jumpToLoadedMatrixEvent(eventId);
                            }

                            function formatRedactedEvent(eventId) {
                                const targetEventId = String(eventId || timelineItemDelegate.eventId || "");
                                const targetRow = TimelineManager.matrixTimelineModel
                                    ? TimelineManager.matrixTimelineModel.rowForEventId(targetEventId)
                                    : -1;
                                if (targetRow >= 0) {
                                    const item = TimelineManager.matrixTimelineModel.itemAt(targetRow);
                                    if (item && item !== undefined) {
                                        return root.matrixRedactedEventPair(String(item.senderDisplayName || ""),
                                                                            String(item.senderId || ""));
                                    }
                                }

                                return root.matrixRedactedEventPair(timelineItemDelegate.senderDisplayName,
                                                                    timelineItemDelegate.senderId);
                            }

                            function copyLinkToEvent(eventId) {
                                TimelineManager.copyMatrixEventLink(
                                    roomId,
                                    String(eventId || timelineItemDelegate.eventId || ""));
                            }

                            function openForwardDialog(eventId) {
                                root.openMatrixForwardDialog(
                                    String(eventId || timelineItemDelegate.eventId || ""));
                            }

                            function markEventAsRead(eventId) {
                                TimelineManager.markActiveMatrixTimelineEventAsRead(
                                    String(eventId || timelineItemDelegate.eventId || ""));
                            }

                            function pin(eventId) {
                                TimelineManager.pinActiveMatrixTimelineEvent(
                                    String(eventId || timelineItemDelegate.eventId || ""));
                            }

                            function unpin(eventId) {
                                TimelineManager.unpinActiveMatrixTimelineEvent(
                                    String(eventId || timelineItemDelegate.eventId || ""));
                            }

                            function reportEvent(eventId, reason, score) {
                                TimelineManager.reportActiveMatrixTimelineEvent(
                                    String(eventId || timelineItemDelegate.eventId || ""),
                                    String(reason || ""),
                                    Number(score || -50));
                            }

                            function viewRawMessage(eventId) {
                                root.openRawMessageDialog(
                                    String(eventId || timelineItemDelegate.eventId || ""));
                            }

                            function viewDecryptedRawMessage(eventId) {
                                viewRawMessage(eventId);
                            }

                            function showReadReceipts(eventId) {
                                root.openReadReceiptsDialog(
                                    String(eventId || timelineItemDelegate.eventId || ""));
                            }
                        }

                        QtObject {
                            id: matrixToolbarMessageModel

                            readonly property string eventId: timelineItemDelegate.eventId
                            readonly property string threadId: timelineItemDelegate.threadId
                            readonly property int type: timelineItemDelegate.matrixEventType
                            readonly property bool isSender: timelineItemDelegate.isOwn
                            readonly property bool isEncrypted: timelineItemDelegate.mediaIsEncrypted || timelineItemDelegate.thumbnailIsEncrypted || timelineItemDelegate.itemKind === "unable_to_decrypt"
                            readonly property string userId: timelineItemDelegate.senderId
                            readonly property string userName: timelineItemDelegate.senderDisplayName
                            readonly property bool isEditable: !root.hasPendingAttachments
                                && !TimelineManager.matrixTimelineAttachmentSending
                                && timelineItemDelegate.isOwn
                                && ["message", "notice", "emote"].indexOf(timelineItemDelegate.itemKind) >= 0
                            readonly property bool isStateEvent: timelineItemDelegate.isStateLikeItem
                            readonly property string body: timelineItemDelegate.body
                            readonly property string formattedBody: timelineItemDelegate.usesSharedStateBubble
                                ? timelineItemDelegate.sharedStatePreviewData.formattedStateEvent
                                : timelineItemDelegate.sharedPreviewData.formattedBody
                            readonly property bool supportsReaction: timelineItemDelegate.supportsSharedToolbarActions
                            readonly property bool supportsReply: timelineItemDelegate.supportsSharedToolbarActions
                            readonly property bool supportsThread: timelineItemDelegate.supportsSharedToolbarActions
                            readonly property bool supportsForward: ["message", "notice", "emote", "image", "video", "audio", "file"].indexOf(timelineItemDelegate.itemKind) >= 0
                            readonly property bool supportsGoToMessage: false
                            readonly property bool supportsOptions: eventId.length > 0
                            readonly property bool supportsEdit: isEditable
                            readonly property bool supportsRemove: eventId.length > 0
                                && timelineItemDelegate.itemKind !== "redacted"
                                && (TimelineManager.matrixTimelineCanRedactOther
                                    || (timelineItemDelegate.isOwn
                                        && TimelineManager.matrixTimelineCanRedactOwn))
                            readonly property bool supportsViewRaw: eventId.length > 0
                            readonly property bool supportsReadReceipts: timelineItemDelegate.isOwn
                                && timelineItemDelegate.supportsSharedToolbarActions
                            readonly property bool supportsMarkAsRead: timelineItemDelegate.supportsSharedToolbarActions
                            readonly property bool supportsPin: timelineItemDelegate.supportsSharedToolbarActions
                            readonly property bool supportsReport: timelineItemDelegate.supportsSharedToolbarActions
                            readonly property bool supportsOpenMedia: timelineItemDelegate.isMediaItem
                            readonly property bool supportsSaveMedia: timelineItemDelegate.isMediaItem
                            readonly property bool supportsCopyEventLink: eventId.length > 0
                        }

                        Rectangle {
                            id: dateDivider

                            anchors.horizontalCenter: parent.horizontalCenter
                            color: palette.mid
                            implicitHeight: dividerLabel.implicitHeight + Komai.paddingSmall * 2
                            height: implicitHeight
                            radius: height / 2
                            visible: itemKind === "date_divider"
                            width: dividerLabel.implicitWidth + Komai.paddingLarge * 2

                            MatrixText {
                                id: dividerLabel

                                anchors.centerIn: parent
                                color: palette.base
                                text: Qt.formatDateTime(new Date(timestamp), "dddd, d MMMM")
                                textFormat: TextEdit.PlainText
                            }
                        }

                        Component {
                            id: matrixPlainMessageStyle

                            TimelinePlainMessageStyle {
                                eventId: timelineItemDelegate.eventId
                                replyTo: !timelineItemDelegate.usesSharedStateBubble
                                    ? timelineItemDelegate.replyEventId
                                    : ""
                                room: null
                                index: timelineItemDelegate.modelIndex
                                day: timelineItemDelegate.dayKey
                                isSender: timelineItemDelegate.isOwn
                                isStateEvent: timelineItemDelegate.usesSharedStateBubble
                                timestamp: new Date(Number(timelineItemDelegate.timestamp))
                                userId: timelineItemDelegate.senderId
                                userName: timelineItemDelegate.senderDisplayName
                                threadId: timelineItemDelegate.threadId
                                userPowerlevel: 0
                                isEdited: timelineItemDelegate.isEdited
                                isEncrypted: timelineItemDelegate.mediaIsEncrypted
                                    || timelineItemDelegate.thumbnailIsEncrypted
                                reactions: timelineItemDelegate.usesSharedStateBubble
                                    ? []
                                    : timelineItemDelegate.reactions
                                status: timelineItemDelegate.sharedDeliveryStatus
                                trustlevel: 0
                                notificationlevel: MtxEvent.Empty
                                type: timelineItemDelegate.usesSharedStateBubble
                                    ? MtxEvent.Name
                                    : timelineItemDelegate.matrixEventType
                                isEditable: timelineItemDelegate.usesSharedTextBubble
                                    && matrixToolbarMessageModel.isEditable
                                isHiddenEvent: false
                                formattedBody: !timelineItemDelegate.usesSharedStateBubble
                                    ? timelineItemDelegate.sharedPreviewData.formattedBody
                                    : ""
                                formattedStateEvent: timelineItemDelegate.usesSharedStateBubble
                                    ? timelineItemDelegate.sharedStatePreviewData.formattedStateEvent
                                    : ""
                                stateEventIconSource: timelineItemDelegate.usesSharedStateBubble
                                    ? timelineItemDelegate.sharedStatePreviewData.stateEventIconSource
                                    : ""
                                messageContextMenu: matrixMessageContextMenu
                                replyContextMenu: matrixReplyContextMenu
                                messageActions: matrixMessageActionsHost.control
                                previewData: timelineItemDelegate.usesSharedStateBubble
                                    ? timelineItemDelegate.sharedStatePreviewData
                                    : (timelineItemDelegate.usesSharedImageBubble
                                        || timelineItemDelegate.usesSharedStickerBubble
                                        || timelineItemDelegate.usesSharedVideoBubble)
                                        ? timelineItemDelegate.sharedVisualPreviewData
                                        : (timelineItemDelegate.usesSharedFileBubble || timelineItemDelegate.usesSharedAudioBubble)
                                        ? timelineItemDelegate.sharedAttachmentPreviewData
                                        : timelineItemDelegate.sharedPreviewData
                                replyPreviewData: !timelineItemDelegate.usesSharedStateBubble
                                    ? timelineItemDelegate.sharedReplyPreviewData
                                    : ({})
                                roomModelOverride: matrixToolbarRoomModel
                                scrolledToThis: false
                            }
                        }

                        Component {
                            id: matrixBubbleMessageStyle

                            TimelineBubbleMessageStyle {
                                eventId: timelineItemDelegate.eventId
                                replyTo: !timelineItemDelegate.usesSharedStateBubble
                                    ? timelineItemDelegate.replyEventId
                                    : ""
                                room: null
                                index: timelineItemDelegate.modelIndex
                                day: timelineItemDelegate.dayKey
                                isSender: timelineItemDelegate.isOwn
                                isStateEvent: timelineItemDelegate.usesSharedStateBubble
                                timestamp: new Date(Number(timelineItemDelegate.timestamp))
                                userId: timelineItemDelegate.senderId
                                userName: timelineItemDelegate.senderDisplayName
                                threadId: timelineItemDelegate.threadId
                                userPowerlevel: 0
                                isEdited: timelineItemDelegate.isEdited
                                isEncrypted: timelineItemDelegate.mediaIsEncrypted
                                    || timelineItemDelegate.thumbnailIsEncrypted
                                reactions: timelineItemDelegate.usesSharedStateBubble
                                    ? []
                                    : timelineItemDelegate.reactions
                                status: timelineItemDelegate.sharedDeliveryStatus
                                trustlevel: 0
                                notificationlevel: MtxEvent.Empty
                                type: timelineItemDelegate.usesSharedStateBubble
                                    ? MtxEvent.Name
                                    : timelineItemDelegate.matrixEventType
                                isEditable: timelineItemDelegate.usesSharedTextBubble
                                    && matrixToolbarMessageModel.isEditable
                                isHiddenEvent: false
                                formattedBody: !timelineItemDelegate.usesSharedStateBubble
                                    ? timelineItemDelegate.sharedPreviewData.formattedBody
                                    : ""
                                formattedStateEvent: timelineItemDelegate.usesSharedStateBubble
                                    ? timelineItemDelegate.sharedStatePreviewData.formattedStateEvent
                                    : ""
                                stateEventIconSource: timelineItemDelegate.usesSharedStateBubble
                                    ? timelineItemDelegate.sharedStatePreviewData.stateEventIconSource
                                    : ""
                                messageContextMenu: matrixMessageContextMenu
                                replyContextMenu: matrixReplyContextMenu
                                messageActions: matrixMessageActionsHost.control
                                previewData: timelineItemDelegate.usesSharedStateBubble
                                    ? timelineItemDelegate.sharedStatePreviewData
                                    : (timelineItemDelegate.usesSharedImageBubble
                                        || timelineItemDelegate.usesSharedStickerBubble
                                        || timelineItemDelegate.usesSharedVideoBubble)
                                        ? timelineItemDelegate.sharedVisualPreviewData
                                        : (timelineItemDelegate.usesSharedFileBubble || timelineItemDelegate.usesSharedAudioBubble)
                                        ? timelineItemDelegate.sharedAttachmentPreviewData
                                        : timelineItemDelegate.sharedPreviewData
                                replyPreviewData: !timelineItemDelegate.usesSharedStateBubble
                                    ? timelineItemDelegate.sharedReplyPreviewData
                                    : ({})
                                roomModelOverride: matrixToolbarRoomModel
                                scrolledToThis: false
                            }
                        }

                        Loader {
                            id: sharedTimelineBubble

                            active: timelineItemDelegate.usesSharedTimelineBubble
                                && !timelineItemDelegate.sharedBubbleReloadArmed
                            asynchronous: true
                            sourceComponent: Settings.timelineMessagesStyle === Settings.TimelineMessagesStyle.Plain
                                ? matrixPlainMessageStyle
                                : matrixBubbleMessageStyle
                            visible: active
                        }
                    }
                }

                ColumnLayout {
                    anchors.centerIn: parent
                    spacing: Komai.paddingMedium
                    visible: !root.hasTimeline
                    width: Math.min(parent.width - Komai.paddingLarge * 2, 560)

                    MatrixText {
                        Layout.fillWidth: true
                        horizontalAlignment: TextEdit.AlignHCenter
                        text: root.loading
                            ? qsTr("Loading room timeline…")
                            : qsTr("No timeline items are loaded for this room yet.")
                        wrapMode: Text.WordWrap
                    }

                    MatrixText {
                        Layout.fillWidth: true
                        color: palette.buttonText
                        horizontalAlignment: TextEdit.AlignHCenter
                        text: qsTr("This room is now backed by the Rust matrix-sdk timeline and shared Komai composer surface while the remaining gaps are migrated.")
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Composer.UploadBox {
                Layout.minimumHeight: 0
                Layout.preferredHeight: layoutVisible && !root.walkModeActive ? implicitHeight : 0
                Layout.maximumHeight: layoutVisible && !root.walkModeActive ? implicitHeight : 0
                uploadsController: matrixUploadsController
                uploadsSending: TimelineManager.matrixTimelineAttachmentSending
            }

            Composer.ReplyPopup {
                Layout.minimumHeight: 0
                Layout.preferredHeight: layoutVisible && !root.walkModeActive ? implicitHeight : 0
                Layout.maximumHeight: layoutVisible && !root.walkModeActive ? implicitHeight : 0
                matrixReplyEventId: TimelineManager.matrixTimelineReplyEventId
                matrixReplySenderId: TimelineManager.matrixTimelineReplySenderId
                matrixReplyDisplayName: TimelineManager.matrixTimelineReplySenderDisplayName
                matrixReplyBody: TimelineManager.matrixTimelineReplyBody
                matrixEditEventId: TimelineManager.matrixTimelineEditEventId
                roomModel: matrixComposerRoom
                roundTopCorners: true
            }

            Rectangle {
                id: composerContainer

                readonly property int contentHeight: root.walkModeActive
                    ? root.composerBaselineHeight
                    : Math.max(root.composerBaselineHeight, composerInput.implicitHeight)
                Layout.fillWidth: true
                Layout.minimumHeight: implicitHeight
                Layout.preferredHeight: implicitHeight
                Layout.maximumHeight: implicitHeight
                color: palette.window
                implicitHeight: inputShellSeparator.implicitHeight + contentHeight

                ColumnLayout {
                    id: composerLayout

                    anchors.fill: parent
                    spacing: 0

                    TimelineSeparator {
                        id: inputShellSeparator

                        Layout.minimumHeight: implicitHeight
                        Layout.preferredHeight: implicitHeight
                        Layout.maximumHeight: implicitHeight
                    }

                    Composer.MessageInput {
                        id: composerInput

                        Layout.fillWidth: true
                        Layout.minimumHeight: visible ? root.composerBaselineHeight : 0
                        Layout.preferredHeight: visible ? Math.max(root.composerBaselineHeight, implicitHeight) : 0
                        Layout.maximumHeight: visible ? Math.max(root.composerBaselineHeight, implicitHeight) : 0
                        room: matrixComposerRoom
                        timelineRoot: root.timelineRoot ? root.timelineRoot : (root.chatRoot ? root.chatRoot : root)
                        selectionModeRoot: root
                        walkModeActive: root.walkModeActive
                        inputController: matrixComposerInputController
                        allowCalls: false
                        allowStickers: false
                        allowCommandCompleter: false
                        attachmentsEnabled: !root.editing
                        showAllButtons: true
                        visible: !root.walkModeActive
                    }

                    TimelineWalkModeBar {
                        Layout.fillWidth: true
                        Layout.minimumHeight: visible ? root.composerBaselineHeight : 0
                        Layout.preferredHeight: visible ? root.composerBaselineHeight : 0
                        Layout.maximumHeight: visible ? root.composerBaselineHeight : 0
                        minimumHeight: root.composerBaselineHeight
                        chatRoot: root
                        visible: root.walkModeActive
                    }
                }
            }
        }
    }

    Connections {
        function onMatrixTimelineStateChanged() {
            root.ensureInitialBottomPin();
            if (root.deferredInitialBufferTopUpPending)
                deferredBufferCheckTimer.restart();
            else
                bufferCheckTimer.restart();

            if (!root.restoringEditDraft || root.activeEditEventId.length > 0)
                return;

            matrixComposerInputController.setText(root.draftBeforeEdit);
            root.draftBeforeEdit = "";
            root.restoringEditDraft = false;
            root.focusTextInput();
        }

        function onFocusInput() {
            root.focusTextInput();
        }

        target: TimelineManager
    }

    Shortcut {
        sequences: [StandardKey.Cancel, "Escape"]
        context: Qt.ApplicationShortcut
        enabled: root.visible && (root.walkModeActive || root.hasSelectedEvents || root.hasFocusedEvent)

        onActivated: root.handleEscape()
    }

    TimelineKeyboardShortcuts {
        chatList: matrixTimelineList
        chatRoot: root
        roomModel: null
    }

    onActiveRoomIdChanged: {
        measuredTimelineHeights = ({});
        initialBottomPinPending = activeRoomId.length > 0;
        initialTimelineBufferPending = activeRoomId.length > 0;
        deferredInitialBufferTopUpPending = false;
        bufferPaginationInFlight = false;
        perfLoggedCountNonZero = false;
        perfLoggedContentHeightReady = false;
        perfLoggedUsefulHeightReady = false;
        perfLoggedBufferFilled = false;
        preferLatestReadMarkerEvent = false;
        lastMarkedReadEventId = "";
        lastInitialBufferTriggerCount = -1;
        visibleTimelineDelegates = ({});
        deferredBufferCheckTimer.stop();
        if (activeRoomId.length > 0)
            root.markRoomSwitchPerfPhase("qml.matrix_room.active_room_changed");
        if (!matrixTimelineList)
            return;

        matrixTimelineList.keepPinnedToBottom = true;
        matrixTimelineList.userUnpinned = false;
        matrixTimelineList.savedTopIndex = -1;
        matrixTimelineList.previousCount = 0;
        matrixTimelineList.visibleIndicesValid = false;
        matrixTimelineList.stableThumbSize = 1.0;
    }

    onLoadingChanged: {
        if (!loading) {
            root.markRoomSwitchPerfPhase("qml.matrix_room.loading_false");
            ensureInitialBottomPin();
            if (root.deferredInitialBufferTopUpPending)
                deferredBufferCheckTimer.restart();
            else
                bufferCheckTimer.restart();
        }
    }
}
