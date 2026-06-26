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

        const model = rootItem.activeTimelineModel;
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

        // Position the ListView by *its own* model's row index. When a
        // filter proxy is interposed (search / collapse-thread-replies) the
        // proxy's row space differs from the raw timeline model's, so reusing
        // `matrixTimelineRowForEventId()` (which walks `activeTimelineModel`)
        // here scrolls to the wrong place — the "weird jumps" of issue #139.
        const listModel = timelineList.model;
        const listRow = (listModel && typeof listModel.rowForEventId === "function")
            ? listModel.rowForEventId(String(eventId || ""))
            : -1;
        if (listRow < 0)
            return;

        timelineList.forceLayout();
        timelineList.positionViewAtIndex(listRow, ListView.Contain);
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

    // Drag-select gesture state — alive only for the duration of a single
    // left-button drag inside a message bubble's litehtml. `_dragSelectStartEventId`
    // holds the source row's eventId between `Began` and `Ended`; latching
    // (`_dragSelectLatched`) flips on the first time the cursor crosses into
    // a different row. `_dragSelectAdditive` is captured at `Began` from the
    // press-time modifiers — Ctrl/Meta/Shift means "add to existing
    // selection" (the snapshot in `_dragSelectBaseSelection` is then merged
    // with each range update); no modifier means "replace".
    property string _dragSelectStartEventId: ""
    property bool _dragSelectLatched: false
    property bool _dragSelectAdditive: false
    property string _dragSelectLastEndpointEventId: ""
    property var _dragSelectBaseSelection: []
    // Auto-scroll bookkeeping: last cursor position and originating litehtml
    // so the timer can keep re-resolving the row under the cursor while it
    // nudges contentY past the viewport edge. `_dragSelectAutoScrollVelocity`
    // is the signed pixel delta per tick (negative = scroll toward older
    // messages); 0 means the timer should stop.
    property point _dragSelectLastScenePos: Qt.point(0, 0)
    property var _dragSelectLastSourceItem: null
    property real _dragSelectAutoScrollVelocity: 0

    Timer {
        id: dragSelectAutoScrollTimer
        interval: 16
        repeat: true
        onTriggered: support._tickDragSelectAutoScroll()
    }

    function dragSelectGestureBegan(eventId, modifiers) {
        _dragSelectStartEventId        = String(eventId || "");
        _dragSelectLatched             = false;
        _dragSelectLastEndpointEventId = "";
        const m = Number(modifiers || 0);
        _dragSelectAdditive            = !!(m & (Qt.ControlModifier | Qt.MetaModifier | Qt.ShiftModifier));
        _dragSelectBaseSelection       = [];
    }

    // Build `selectedEventIds` from range(drag-anchor → endpoint), prefixed
    // with the pre-drag snapshot if the gesture is additive. Used by both the
    // trip path and latched updates so shrink/grow stays consistent.
    function _applyDragSelectRange(endpointEventId) {
        const anchorEventId = _dragSelectStartEventId;
        if (anchorEventId.length === 0)
            return false;

        const model = rootItem.activeTimelineModel;
        if (!model)
            return false;

        const anchorRow = model.rowForEventId(anchorEventId);
        const endRow    = model.rowForEventId(endpointEventId);
        if (anchorRow < 0 || endRow < 0)
            return false;

        const minRow = Math.min(anchorRow, endRow);
        const maxRow = Math.max(anchorRow, endRow);

        const seen = {};
        const next = [];

        const base = _dragSelectBaseSelection;
        for (let i = 0; i < base.length; i += 1) {
            const eid = String(base[i] || "");
            if (eid.length > 0 && !seen[eid]) {
                seen[eid] = true;
                next.push(eid);
            }
        }

        for (let row = minRow; row <= maxRow; row += 1) {
            if (!rootItem.isSelectableMatrixTimelineRow(row))
                continue;
            const item = model.itemAt(row);
            if (!item)
                continue;
            const eid = String(item.eventId || "");
            if (eid.length === 0 || seen[eid])
                continue;
            seen[eid] = true;
            next.push(eid);
        }

        rootItem.selectedEventIds = next;
        return true;
    }

    function dragSelectGestureMoved(eventId, scenePos, sourceItem) {
        if (!Settings.timelineMessagesDragSelect)
            return false;

        if (_dragSelectStartEventId.length === 0 || !timelineList)
            return false;

        const contentItem = timelineList.contentItem;
        if (!contentItem)
            return false;

        // Remember the last cursor position + the source litehtml (if any)
        // so the auto-scroll timer can keep re-resolving the row under the
        // cursor as it nudges contentY, even while no more pointer events
        // arrive from the OS.
        _dragSelectLastScenePos    = scenePos;
        _dragSelectLastSourceItem  = sourceItem || null;

        // When the cursor leaves the viewport vertically, stop trying to
        // resolve a row from the current frame and hand the gesture over to
        // the auto-scroll timer. The timer keeps shifting contentY in the
        // appropriate direction and re-runs the row-resolve step against
        // each frame's new content layout.
        const local = timelineList.mapFromItem(null, scenePos.x, scenePos.y);
        const velocity = _dragSelectAutoScrollVelocityForLocal(local.y, timelineList.height);
        if (velocity !== 0) {
            _dragSelectAutoScrollVelocity = velocity;
            if (!dragSelectAutoScrollTimer.running)
                dragSelectAutoScrollTimer.start();
            return true;
        }
        // Cursor is back inside the viewport — stop any auto-scroll in
        // flight and resolve the row from the live cursor position.
        if (dragSelectAutoScrollTimer.running) {
            dragSelectAutoScrollTimer.stop();
            _dragSelectAutoScrollVelocity = 0;
        }

        return _resolveDragSelectEndpointAndApply(scenePos, sourceItem);
    }

    // Extracted so the auto-scroll timer can re-run the row-resolve step at
    // the cursor's last known scene position, against the contentItem layout
    // after each tick's contentY nudge.
    function _resolveDragSelectEndpointAndApply(scenePos, sourceItem) {
        const contentItem = timelineList ? timelineList.contentItem : null;
        if (!contentItem)
            return false;

        // `indexAt` expects coordinates in the same space as delegate x/y,
        // i.e. the contentItem's local coords. With `verticalLayoutDirection:
        // BottomToTop` ListView.contentY can be negative, so `local + contentY`
        // is unreliable — map through contentItem directly.
        const contentPt = contentItem.mapFromItem(null, scenePos.x, scenePos.y);
        const row = timelineList.indexAt(contentPt.x, contentPt.y);
        if (row < 0)
            return false;

        const endpointEventId = String(rootItem.selectableEventIdNearMatrixRow(row) || "");
        if (endpointEventId.length === 0)
            return false;

        if (!_dragSelectLatched) {
            // Still inside the start row — keep the gesture as text selection.
            if (endpointEventId === _dragSelectStartEventId)
                return false;

            const anchorEventId = _dragSelectStartEventId;
            if (!rootItem.canExplicitlySelectEventId(anchorEventId))
                return false;

            // Escalate: tell the originating litehtml to drop its in-progress
            // text selection and silence subsequent text-selection updates.
            if (sourceItem && typeof sourceItem.suppressTextSelection === "function")
                sourceItem.suppressTextSelection();

            if (_dragSelectAdditive) {
                // Snapshot the pre-drag selection so subsequent shrink/grow
                // keeps it intact. Don't `clearWalkState` — that would wipe
                // the prior selection we're explicitly preserving.
                _dragSelectBaseSelection = rootItem.selectedEventIds.slice();
            } else {
                // File-manager convention: a plain drag is a fresh selection.
                clearWalkState({
                    "focusComposer": false
                });
                _dragSelectBaseSelection = [];
            }
            rootItem.walkModeActive         = true;
            rootItem.selectionAnchorEventId = anchorEventId;
            if (!focusWalkModeEventById(endpointEventId, {
                    "skipScroll": true
                })) {
                return false;
            }
            _applyDragSelectRange(endpointEventId);
            _dragSelectLatched             = true;
            _dragSelectLastEndpointEventId = endpointEventId;
            return true;
        }

        // Latched: just track the cursor.
        if (endpointEventId === _dragSelectLastEndpointEventId)
            return true;

        if (!focusWalkModeEventById(endpointEventId, {
                "skipScroll": true
            })) {
            return false;
        }
        _applyDragSelectRange(endpointEventId);
        _dragSelectLastEndpointEventId = endpointEventId;
        return true;
    }

    // Velocity (pixels per timer tick, signed for contentY direction) that
    // matches a wheel-tick of similar visual speed. Negative scrolls toward
    // older messages (the visual top in BottomToTop), positive toward newer.
    readonly property int _dragSelectAutoScrollMaxPx: 16

    function _dragSelectAutoScrollVelocityForLocal(localY, viewportHeight) {
        if (localY < 0) {
            // Above the viewport — scroll toward older messages. The further
            // past the edge, the faster (linear ramp, capped).
            const distance = Math.min(120, -localY);
            return -Math.max(2, Math.min(_dragSelectAutoScrollMaxPx, distance / 4));
        }
        if (localY > viewportHeight) {
            const distance = Math.min(120, localY - viewportHeight);
            return Math.max(2, Math.min(_dragSelectAutoScrollMaxPx, distance / 4));
        }
        return 0;
    }

    function _tickDragSelectAutoScroll() {
        if (!timelineList || _dragSelectAutoScrollVelocity === 0) {
            dragSelectAutoScrollTimer.stop();
            _dragSelectAutoScrollVelocity = 0;
            return;
        }

        const range = Math.max(0, timelineList.contentHeight - timelineList.height);
        if (range <= 0) {
            // Nothing to scroll — nothing to do.
            dragSelectAutoScrollTimer.stop();
            _dragSelectAutoScrollVelocity = 0;
            return;
        }

        // Match the wheel handler's clamping pattern (see
        // MatrixRoomListShellSupport.handleWheelRotation) so over-scroll
        // jitter near the edges doesn't show up here either.
        const minY = timelineList.originY;
        const maxY = timelineList.originY + range;
        const proposed = timelineList.contentY + _dragSelectAutoScrollVelocity;
        const clamped = Math.max(minY, Math.min(maxY, proposed));
        if (clamped === timelineList.contentY) {
            // Hit a bound — keep the timer running but don't keep emitting
            // pointless contentY writes; the user will lift the button or
            // drag back in and we'll cancel naturally.
            return;
        }
        timelineList.contentY = clamped;

        if (!timelineList.isNearLiveEdge()) {
            timelineList.keepPinnedToBottom = false;
            timelineList.userUnpinned = true;
        }

        // Re-resolve the row under the cursor — the cursor hasn't moved but
        // the content under it just shifted, so the row is different.
        _resolveDragSelectEndpointAndApply(_dragSelectLastScenePos,
                                           _dragSelectLastSourceItem);
    }

    function dragSelectGestureEnded() {
        _dragSelectStartEventId        = "";
        _dragSelectLatched             = false;
        _dragSelectAdditive            = false;
        _dragSelectLastEndpointEventId = "";
        _dragSelectBaseSelection       = [];
        _dragSelectLastSourceItem      = null;
        _dragSelectAutoScrollVelocity  = 0;
        dragSelectAutoScrollTimer.stop();
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

        return focusWalkModeEventById(bottomMostEventId, {
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
        const model = rootItem.activeTimelineModel;
        if (normalizedEventId.length === 0 || !model)
            return -1;

        return model.rowForEventId(normalizedEventId);
    }

    function activeTimelineRowCount() {
        const model = rootItem.activeTimelineModel;
        return model ? model.count : 0;
    }

    function isSelectableMatrixTimelineRow(row) {
        const model = rootItem.activeTimelineModel;
        if (!model || row < 0 || row >= model.count)
            return false;

        const item = model.itemAt(row);
        if (!item
            || String(item.eventId || "").length === 0
            || String(item.typeString || "") === "date_divider"
            || Boolean(item.isHiddenEvent))
            return false;

        // Skip collapsed thread replies — they are hidden from the *room*
        // timeline. Inside a thread view every reply is shown, so the
        // collapse preference does not apply there.
        if (!rootItem.threadViewActive
                && rootItem.filteredTimeline.collapseThreadReplies
                && String(item.threadId || "").length > 0
                && !Boolean(item.isThreadRoot))
            return false;

        return true;
    }

    function focusMatrixTimelineRow(row, options) {
        if (!isSelectableMatrixTimelineRow(row))
            return false;

        const item = rootItem.activeTimelineModel.itemAt(row);
        return focusWalkModeEventById(String(item.eventId || ""), options || {});
    }

    function moveFocusByStep(step) {
        const currentRow = matrixTimelineRowForEventId(rootItem.focusedEventId);
        if (currentRow < 0)
            return false;

        const rowCount = activeTimelineRowCount();
        for (let row = currentRow + step; row >= 0 && row < rowCount; row += step) {
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

        const rowCount = activeTimelineRowCount();
        let remaining = walkModeChunkSize();
        for (let row = currentRow + step; row >= 0 && row < rowCount; row += step) {
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
        for (let row = activeTimelineRowCount() - 1; row >= 0; row--) {
            if (focusMatrixTimelineRow(row, options || {}))
                return true;
        }

        return false;
    }

    function focusLatestWalkModeEvent(options) {
        const rowCount = activeTimelineRowCount();
        for (let row = 0; row < rowCount; row++) {
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
        const model = rootItem.activeTimelineModel;
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
        const model = rootItem.activeTimelineModel;
        if (!model)
            return [];

        // Order against the full timeline (rawRowForEventId) rather than the
        // revealed window (rowForEventId): a selected event scrolled out of (or
        // re-hidden from) the visible window still exists in the timeline and
        // must survive into the copy, not be silently dropped. Falls back to the
        // visible-window lookup for models that don't expose the raw variant
        // (e.g. the search/collapse filter proxy).
        const hasRawRow = typeof model.rawRowForEventId === "function";

        const entries = [];
        for (let i = 0; i < eventIds.length; i++) {
            const eid = String(eventIds[i] || "");
            if (eid.length === 0)
                continue;

            const row = hasRawRow ? model.rawRowForEventId(eid) : model.rowForEventId(eid);
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
