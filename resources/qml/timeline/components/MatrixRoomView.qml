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
    property bool initialBottomPinPending: false
    property bool initialTimelineBufferPending: false

    MessageActionSupport {
        id: messageActionSupport
    }

    function clearSearch() {
        if (root.chatRoot && typeof root.chatRoot.clearSearch === "function")
            root.chatRoot.clearSearch();
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
        focusTimelineSelection();

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

        const delegateItem = bottomMostVisibleDelegate();
        clearWalkState({
            "focusComposer": false
        });
        if (!delegateItem || !delegateItem.eventId)
            return focusLatestWalkModeEvent();

        return focusWalkModeEventById(String(delegateItem.eventId || ""), {
                "skipScroll": true
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
        if (!walkModeActive && selectedEventIds.length === 0)
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
        return moveFocusByStep(-1);
    }

    function moveFocusTowardNewerEvents() {
        return moveFocusByStep(1);
    }

    function moveFocusTowardOlderEventsByChunk() {
        return moveFocusByChunk(-1);
    }

    function moveFocusTowardNewerEventsByChunk() {
        return moveFocusByChunk(1);
    }

    function focusOldestLoadedWalkModeEvent() {
        for (let row = 0; row < TimelineManager.matrixTimelineItemCount; row++) {
            if (focusMatrixTimelineRow(row))
                return true;
        }

        return false;
    }

    function focusLatestWalkModeEvent() {
        for (let row = TimelineManager.matrixTimelineItemCount - 1; row >= 0; row--) {
            if (focusMatrixTimelineRow(row))
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
            matrixTimelineList.keepPinnedToBottom = true;
            matrixTimelineList.maybeScrollToBottom(true);
            matrixTimelineList.updateLastScroll();
            if (matrixTimelineList.atYEnd)
                root.initialBottomPinPending = false;
        });
    }

    function maybeRequestInitialTimelineBuffer() {
        if (!matrixTimelineList
                || !initialTimelineBufferPending
                || loading
                || !hasTimeline)
            return;

        const viewportHeight = matrixTimelineList.height;
        if (viewportHeight <= 0)
            return;

        const desiredBufferedHeight = viewportHeight + Math.min(viewportHeight * 0.25, 320);
        if (matrixTimelineList.contentHeight >= desiredBufferedHeight) {
            initialTimelineBufferPending = false;
            lastInitialBufferTriggerCount = -1;
            return;
        }

        const itemCount = TimelineManager.matrixTimelineItemCount;
        if (itemCount <= 0 || lastInitialBufferTriggerCount === itemCount)
            return;

        if (!TimelineManager.paginateActiveMatrixTimelineBackwards(6)) {
            initialTimelineBufferPending = false;
            lastInitialBufferTriggerCount = -1;
            return;
        }

        lastInitialBufferTriggerCount = itemCount;
    }

    function matrixEventTypeForItemKind(kind) {
        switch (kind) {
        case "notice":
            return MtxEvent.NoticeMessage;
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

    function openRemoveMessageDialog(eventId) {
        const trimmedEventId = String(eventId || "").trim();
        if (trimmedEventId.length === 0)
            return null;

        return showDialogFromComponent(removeReasonDialogComponent, {
                "eventId": trimmedEventId
            });
    }

    function openRawMessageDialog(eventId) {
        const trimmedEventId = String(eventId || "").trim();
        if (trimmedEventId.length === 0)
            return null;

        const payload = TimelineManager.rawMessageDialogForActiveMatrixTimelineEvent(trimmedEventId);
        if (!payload || !payload.rawMessageJson)
            return null;

        return showDialogFromComponent(rawMessageDialogComponent, payload);
    }

    function openReadReceiptsDialog(eventId) {
        const trimmedEventId = String(eventId || "").trim();
        if (trimmedEventId.length === 0)
            return null;

        const readReceipts = TimelineManager.readReceiptsModelForActiveMatrixTimelineEvent(trimmedEventId);
        if (!readReceipts)
            return null;

        return showDialogFromComponent(readReceiptsDialogComponent, {
                "readReceipts": readReceipts,
                "room": matrixDialogRoomModel
            });
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

    QtObject {
        id: matrixUploadsController

        property var uploads: TimelineManager.matrixTimelineAttachments

        function declineUploads() {
            TimelineManager.clearActiveMatrixAttachments();
        }

        function removeUpload(index) {
            TimelineManager.removeActiveMatrixAttachment(index);
        }

        function send() {
            return TimelineManager.sendActiveMatrixAttachments();
        }
    }

    QtObject {
        id: matrixComposerInputController

        property var uploads: TimelineManager.matrixTimelineAttachments
        readonly property bool uploading: TimelineManager.matrixTimelineAttachmentSending
        property string text: ""
        property string commandValidationMessage: ""
        property string commandValidationState: "none"

        function setText(value) {
            text = String(value || "");
        }

        function openFileSelection() {
            return TimelineManager.openActiveMatrixAttachmentSelection();
        }

        function send() {
            return root.trySendMessage();
        }

        function previousText() {
            return text;
        }

        function nextText() {
            return text;
        }

        function updateState(_selectionStart, _selectionEnd, _cursorPosition, value) {
            const normalized = String(value || "");
            if (text !== normalized)
                text = normalized;
        }

        function clipboardText() {
            return Clipboard.text;
        }

        function tryPasteAttachment(_strict) {
            return false;
        }

        function commandCompletionSearchString(prefix, _cursorPosition) {
            return String(prefix || "");
        }

        function applyCommandCompletion(currentText, _cursorPosition, _completion) {
            return String(currentText || "");
        }

        function commandCompletionCursorPosition(_currentText, cursorPosition, _completion) {
            return cursorPosition;
        }

        function addMention(_userId, _completion) {
        }

        function sticker(_row) {
        }
    }

    QtObject {
        id: matrixComposerPermissions

        function canSend(_eventType) {
            return true;
        }
    }

    QtObject {
        id: matrixComposerRoom

        property string roomId: root.roomPreview ? root.roomPreview.roomid : ""
        property bool isEncrypted: root.roomPreview ? !!root.roomPreview.isEncrypted : false
        property int roomMemberCount: root.roomPreview && root.roomPreview.roomMemberCount !== undefined
            ? Number(root.roomPreview.roomMemberCount)
            : 0
        property var permissions: matrixComposerPermissions
        property var input: matrixComposerInputController

        function showEvent(eventId) {
            return root.jumpToLoadedMatrixEvent(eventId);
        }

        function openUserProfile(userId) {
            matrixDialogRoomModel.openUserProfile(userId);
        }
    }

    QtObject {
        id: matrixMessageActionsDefaultPermissions

        function canSend(eventType) {
            return false;
        }

        function canRedact() {
            return false;
        }

        function canChange(eventType) {
            return false;
        }
    }

    QtObject {
        id: matrixMessageActionsDefaultRoomModel

        property string roomId: root.roomPreview ? root.roomPreview.roomid : ""
        property var permissions: matrixMessageActionsDefaultPermissions
        property var input: null
        property var frequentReactions: []
        property var pinnedMessages: TimelineManager.matrixTimelinePinnedEventIds

        function showEvent(eventId) {
            return root.jumpToLoadedMatrixEvent(eventId);
        }

        function openForwardDialog(eventId) {
            return root.openMatrixForwardDialog(eventId);
        }
    }

    MessageContextMenu {
        id: matrixMessageContextMenu

        chatRoot: root
        emojiPopup: root.emojiPopup
        filteredTimelineModel: root.filteredTimeline
    }

    ReplyContextMenu {
        id: matrixReplyContextMenu

        roomModel: matrixMessageActionsDefaultRoomModel
    }

    MessageActionsHost {
        id: matrixMessageActionsHost

        chatList: matrixTimelineList
        chatRoot: root
        emojiPopup: root.emojiPopup
        filteredTimeline: root.filteredTimeline
        roomModel: matrixMessageActionsDefaultRoomModel
    }

    Component {
        id: removeReasonDialogComponent

        InputDialog {
            required property string eventId

            placeholderText: qsTr("Optional reason")
            title: qsTr("Delete this message?")
            titleIcon: ":/icons/icons/ui/delete.svg"
            acceptText: qsTr("Delete")

            onInputAccepted: function (text) {
                TimelineManager.redactActiveMatrixTimelineEvent(eventId, text);
            }
        }
    }

    Component {
        id: rawMessageDialogComponent

        TimelineDialogs.RawMessageDialog {
        }
    }

    Component {
        id: readReceiptsDialogComponent

        TimelineDialogs.ReadReceipts {
        }
    }

    Component {
        id: reportMessageDialogComponent

        ModerationDialogs.ReportMessage {
        }
    }

    Component {
        id: forwardDialogComponent

        NavigationDialogs.ForwardCompleter {
        }
    }

    QtObject {
        id: matrixDialogRoomModel

        property string roomId: root.roomPreview ? root.roomPreview.roomid : ""

        function openUserProfile(userId) {
            const trimmedUserId = String(userId || "").trim();
            if (trimmedUserId.length === 0)
                return;

            TimelineManager.openGlobalUserProfile(trimmedUserId);
        }
    }

    QtObject {
        id: matrixForwardRoomModel

        property string roomId: root.roomPreview ? root.roomPreview.roomid : ""

        function forwardMessage(eventId, targetRoomId) {
            TimelineManager.forwardActiveMatrixTimelineEvent(String(eventId || ""),
                                                            String(targetRoomId || ""));
        }
    }

    function openMatrixForwardDialog(eventId) {
        const trimmedEventId = String(eventId || "").trim();
        if (trimmedEventId.length === 0)
            return null;

        if (root.chatRoot && root.chatRoot.dialogHost
                && typeof root.chatRoot.dialogHost.showForwardMessageDialog === "function") {
            return root.chatRoot.dialogHost.showForwardMessageDialog(matrixForwardRoomModel,
                                                                    [trimmedEventId],
                                                                    null,
                                                                    null,
                                                                    1);
        }

        const dialogParent = root.chatRoot && root.chatRoot.dialogHost
            ? root.chatRoot.dialogHost
            : (root.chatRoot ? root.chatRoot : root);
        const dialog = forwardDialogComponent.createObject(dialogParent, {
                "roomSource": matrixForwardRoomModel,
                "timelineSource": null,
                "timelineViewSource": null,
                "showReplyPreview": false
            });
        if (!dialog)
            return null;

        dialog.setMessageEventIds([trimmedEventId], 1);
        dialog.open();
        root.destroyOnClose(dialog);
        return dialog;
    }

    function openForwardDialog(eventId) {
        return openMatrixForwardDialog(eventId);
    }

    function openReportMessageDialog(eventId) {
        const trimmedEventId = String(eventId || "").trim();
        if (trimmedEventId.length === 0)
            return null;

        return showDialogFromComponent(reportMessageDialogComponent, {
                "eventId": trimmedEventId,
                "room": matrixMessageActionsDefaultRoomModel
            });
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
        const component = Qt.createComponent("qrc:/resources/qml/dialogs/timeline/MessageActionsDialog.qml");
        if (component.status !== Component.Ready) {
            console.error("MessageActionsDialog: " + component.errorString());
            return;
        }

        const dialogParent = root.chatRoot && root.chatRoot.dialogHost
            ? root.chatRoot.dialogHost
            : (root.chatRoot ? root.chatRoot : root);
        const dialog = component.createObject(dialogParent, {
                "eventId": eventId,
                "eventType": eventType,
                "isSender": isSender,
                "isEncrypted": isEncrypted,
                "link": link || "",
                "roomModel": roomModelOverride || matrixMessageActionsDefaultRoomModel,
                "roomModelOverride": roomModelOverride || null,
                "messageModelOverride": messageModelOverride || null,
                "chatRoot": root,
                "appRoot": dialogParent
            });
        if (!dialog)
            return;

        dialog.open();
        root.destroyOnClose(dialog);
    }

    PreviewPermissions {
        id: matrixHeaderPreviewPermissions
    }

    QtObject {
        id: matrixHeaderRoomModel

        property string roomId: root.roomPreview ? root.roomPreview.roomid : ""
        property int roomMemberCount: root.roomPreview && root.roomPreview.memberCount !== undefined
            ? Number(root.roomPreview.memberCount)
            : (root.roomPreview && root.roomPreview.roomMemberCount !== undefined
                ? Number(root.roomPreview.roomMemberCount)
                : 0)
        property var pinnedMessages: TimelineManager.matrixTimelinePinnedEventIds
        property var widgetLinks: []
        property bool isEncrypted: !!root.roomPreview && root.roomPreview.isEncrypted
        property bool isPublic: !root.roomPreview || root.roomPreview.isPublic
        property AbstractPermissions permissions: matrixHeaderPreviewPermissions
        property bool supportsSearch: false
        property bool supportsPinnedMessagesUi: true
        property bool supportsVisibilityInfo: true

        function previewDataForEvent(eventId) {
            const model = TimelineManager.matrixTimelineModel;
            if (!model)
                return ({});

            const row = model.rowForEventId(String(eventId || ""));
            if (row < 0)
                return ({});

            const item = model.itemAt(row);
            if (!item || item.itemKind === undefined)
                return ({});

            const previousItem = row > 0 ? model.itemAt(row - 1) : ({});
            const timestamp = Number(item.timestamp || 0);
            const dayKey = root.matrixTimelineDayKey(timestamp);
            const previousTimestamp = previousItem.timestamp !== undefined
                ? new Date(Number(previousItem.timestamp))
                : new Date(timestamp);
            const previousDay = previousItem.timestamp !== undefined
                ? root.matrixTimelineDayKey(previousItem.timestamp)
                : dayKey;
            const previousIsStateEvent = previousItem.eventId === undefined
                ? true
                : root.isMatrixStateLikeKind(previousItem.itemKind);
            const previousUserId = previousItem.senderId !== undefined
                ? String(previousItem.senderId || "")
                : "";
            const itemKind = String(item.itemKind || "");
            const body = String(item.body || "");
            const effectiveFileName = item.fileName && String(item.fileName).length > 0
                ? String(item.fileName)
                : (body.length > 0 ? body : qsTr("Attachment"));
            const humanReadableMediaSize = Number(item.mediaSizeBytes || 0) > 0
                ? Komai.humanReadableFileSize(Number(item.mediaSizeBytes))
                : "";
            const basePreview = {
                "room": matrixHeaderRoomModel,
                "eventId": String(item.eventId || ""),
                "userId": String(item.senderId || ""),
                "userName": String(item.senderDisplayName || ""),
                "avatarUrl": String(item.senderAvatarUrl || ""),
                "previousDay": previousDay,
                "previousTimestamp": previousTimestamp,
                "previousIsStateEvent": previousIsStateEvent,
                "previousUserId": previousUserId
            };

            if (root.isMatrixStateLikeKind(itemKind)) {
                return Object.assign({}, basePreview, {
                    "type": MtxEvent.Name,
                    "formattedStateEvent": root.formattedMatrixTextHtml(body),
                    "stateEventIconSource": root.matrixStateEventIconForKind(itemKind)
                });
            }

            if (itemKind === "image" || itemKind === "sticker" || itemKind === "video") {
                const mediaWidth = Math.round(Number(item.mediaWidth || 0));
                const mediaHeight = Math.round(Number(item.mediaHeight || 0));
                const safePreviewAspectRatio = mediaWidth > 0 && mediaHeight > 0
                    ? (mediaHeight / mediaWidth)
                    : 0.75;
                return Object.assign({}, basePreview, {
                    "type": root.matrixEventTypeForItemKind(itemKind),
                    "body": body,
                    "url": String(item.mediaUrl || ""),
                    "blurhash": "",
                    "filename": effectiveFileName,
                    "filesize": humanReadableMediaSize,
                    "filesizeBytes": Math.round(Number(item.mediaSizeBytes || 0)),
                    "mimetype": String(item.mimeType || ""),
                    "thumbnailUrl": String(item.thumbnailUrl || ""),
                    "originalWidth": mediaWidth,
                    "originalHeight": mediaHeight,
                    "proportionalHeight": safePreviewAspectRatio,
                    "containerHeight": root.height > 0 ? root.height : Screen.height,
                    "duration": Math.round(Number(item.mediaDurationMs || 0))
                });
            }

            if (itemKind === "file" || itemKind === "audio") {
                return Object.assign({}, basePreview, {
                    "type": root.matrixEventTypeForItemKind(itemKind),
                    "body": body,
                    "filename": effectiveFileName,
                    "filesize": humanReadableMediaSize,
                    "fileTypeIconSource": Komai.fileTypeIconSource(String(item.mimeType || "")),
                    "mimetype": String(item.mimeType || ""),
                    "duration": Math.round(Number(item.mediaDurationMs || 0))
                });
            }

            return Object.assign({}, basePreview, {
                "type": root.matrixEventTypeForItemKind(itemKind),
                "body": body,
                "formattedBody": root.formattedMatrixTextHtml(body),
                "formattedStateEvent": root.formattedMatrixTextHtml(body),
                "stateEventIconSource": root.matrixStateEventIconForKind(itemKind),
                "typeString": itemKind,
                "callType": "",
                "isOnlyEmoji": 0
            });
        }

        function getDump(eventId, _scope) {
            const preview = previewDataForEvent(eventId);
            return {
                "eventId": String(eventId || ""),
                "userId": String((preview && preview.userId) || ""),
                "userName": String((preview && preview.userName) || "")
            };
        }

        function showEvent(eventId) {
            return root.jumpToLoadedMatrixEvent(String(eventId || ""));
        }

        function openUserProfile(userId) {
            matrixDialogRoomModel.openUserProfile(userId);
        }

        function unpin(eventId) {
            TimelineManager.unpinActiveMatrixTimelineEvent(String(eventId || ""));
        }
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
                }

                ListView {
                    id: matrixTimelineList

                    property int delegateMaxWidth: width - (matrixTimelineScrollbar.interactive ? matrixTimelineScrollbar.width : 0)
                    property bool keepPinnedToBottom: true
                    property int previousCount: 0
                    property real lastScrollPos: 0

                    function updateLastScroll() {
                        lastScrollPos = contentY + height;
                    }

                    function updateBottomPin() {
                        if (root.initialBottomPinPending) {
                            keepPinnedToBottom = true;
                            if (atYEnd)
                                root.initialBottomPinPending = false;
                            return;
                        }

                        keepPinnedToBottom = atYEnd;
                    }

                    function maybeScrollToBottom(force) {
                        if (count <= 0)
                            return;

                        if (!(force || keepPinnedToBottom || root.initialBottomPinPending))
                            return;

                        Qt.callLater(function () {
                            if (count <= 0 || !(force || keepPinnedToBottom || root.initialBottomPinPending))
                                return;

                            positionViewAtEnd();
                            updateBottomPin();
                        });
                    }

                    anchors.fill: parent
                    anchors.margins: Komai.paddingLarge
                    anchors.rightMargin: Komai.paddingLarge + (matrixTimelineScrollbar.interactive ? matrixTimelineScrollbar.width : 0)
                    clip: true
                    // The Rust-room delegate stack still wraps the shared timeline surface in an
                    // extra loader/item layer. Under reuseItems that can briefly recycle stale
                    // state/message visuals while new delegate content settles, and it also makes
                    // total-height estimation wobblier than the legacy path. Keep this disabled
                    // until the shared matrix-room delegate stack is flatter.
                    reuseItems: false
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
                            return baseBuffer;

                        const viewportBuffer = Math.max(baseBuffer, matrixTimelineList.height * 2);
                        // Keep some extra resolved content above/below the viewport so delegate
                        // height estimation stays stable, but do not cache the whole room.
                        return Math.min(viewportBuffer, 4096);
                    }
                    model: TimelineManager.matrixTimelineModel
                    ScrollBar.vertical: matrixTimelineScrollbar
                    spacing: Komai.paddingMedium
                    visible: root.hasTimeline

                    Keys.onPressed: event => {
                        root.handleWalkModeKey(event);
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
                            matrixTimelineList.updateBottomPin();
                        }
                    }

                    onMovementEnded: {
                        updateLastScroll();
                        updateBottomPin();
                    }
                    onAtYBeginningChanged: {
                        if (atYBeginning
                                && root.hasTimeline
                                && !root.loading
                                && root.lastPaginationTriggerCount !== TimelineManager.matrixTimelineItemCount) {
                            if (TimelineManager.paginateActiveMatrixTimelineBackwards(0))
                                root.lastPaginationTriggerCount = TimelineManager.matrixTimelineItemCount;
                        }
                    }
                    onContentYChanged: {
                        if (contentY > 0 && root.lastPaginationTriggerCount === TimelineManager.matrixTimelineItemCount)
                            root.lastPaginationTriggerCount = -1;

                        if (!moving && !flicking && !dragging)
                            updateBottomPin();
                    }
                    onContentHeightChanged: {
                        if (!moving && !flicking && !dragging) {
                            maybeScrollToBottom(previousCount === 0);
                            updateLastScroll();
                        }
                        root.maybeRequestInitialTimelineBuffer();
                    }
                    onHeightChanged: {
                        contentY = lastScrollPos - height;
                        if (!moving && !flicking && !dragging) {
                            maybeScrollToBottom(previousCount === 0);
                            updateLastScroll();
                        }
                        root.maybeRequestInitialTimelineBuffer();
                    }
                    onCountChanged: {
                        const forceScroll = previousCount === 0;
                        maybeScrollToBottom(forceScroll);
                        updateLastScroll();
                        root.maybeRequestInitialTimelineBuffer();
                        previousCount = count;
                    }
                    onModelChanged: {
                        updateLastScroll();
                        keepPinnedToBottom = true;
                        previousCount = count;
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
                        readonly property bool supportsSharedToolbarActions: eventId.length > 0 && itemKind !== "date_divider" && !isStateLikeItem
                        readonly property int matrixEventType: root.matrixEventTypeForItemKind(itemKind)
                        readonly property int dayKey: root.matrixTimelineDayKey(timestamp)
                        readonly property var previousItem: TimelineManager.matrixTimelineModel && modelIndex > 0
                            ? TimelineManager.matrixTimelineModel.itemAt(modelIndex - 1)
                            : ({})
                        readonly property string sharedHumanReadableMediaSize: mediaSizeBytes > 0
                            ? Komai.humanReadableFileSize(Number(mediaSizeBytes))
                            : ""
                        readonly property string sharedFileTypeIconSource: Komai.fileTypeIconSource(mimeType)
                        readonly property var sharedPreviewData: ({
                                "room": matrixToolbarRoomModel,
                                "avatarUrl": senderAvatarUrl,
                                "body": body,
                                "formattedBody": root.formattedMatrixTextHtml(body),
                                "isOnlyEmoji": 0,
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
                        readonly property real sharedTimelineHeightEstimate: {
                            if (itemKind === "date_divider")
                                return dateDivider.implicitHeight;
                            if (!usesSharedTimelineBubble)
                                return 0;
                            if (sharedTimelineBubble.item) {
                                const resolvedHeight = sharedTimelineBubble.item.implicitHeight > 0
                                    ? sharedTimelineBubble.item.implicitHeight
                                    : sharedTimelineBubble.item.height;
                                if (resolvedHeight > 0)
                                    return resolvedHeight;
                            }
                            return modelIndex === 0 ? 10 : 100;
                        }
                        width: matrixTimelineList.width
                        height: sharedTimelineHeightEstimate

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
                                status: MtxEvent.Empty
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
                                status: MtxEvent.Empty
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

            Rectangle {
                id: composerContainer

                Layout.fillWidth: true
                Layout.minimumHeight: implicitHeight
                Layout.preferredHeight: implicitHeight
                Layout.maximumHeight: implicitHeight
                color: palette.window
                implicitHeight: root.walkModeActive
                    ? walkModeBar.implicitHeight
                    : composerLayout.implicitHeight + Komai.paddingMedium * 2

                Rectangle {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    color: palette.mid
                    height: 1
                }

                ColumnLayout {
                    id: composerLayout

                    anchors.fill: parent
                    anchors.margins: root.walkModeActive ? 0 : Komai.paddingMedium
                    spacing: root.walkModeActive ? 0 : Komai.paddingSmall

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

                    Composer.MessageInput {
                        id: composerInput

                        Layout.fillWidth: true
                        Layout.minimumHeight: visible ? Math.max(48, Komai.navigationRowHeight) : 0
                        Layout.preferredHeight: visible ? Math.max(Math.max(48, Komai.navigationRowHeight), implicitHeight) : 0
                        Layout.maximumHeight: visible ? Math.max(Math.max(48, Komai.navigationRowHeight), implicitHeight) : 0
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
                        Layout.minimumHeight: visible ? Math.max(48, Komai.navigationRowHeight) : 0
                        Layout.preferredHeight: visible ? Math.max(48, Komai.navigationRowHeight) : 0
                        Layout.maximumHeight: visible ? Math.max(48, Komai.navigationRowHeight) : 0
                        minimumHeight: Math.max(48, Komai.navigationRowHeight)
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
            root.maybeRequestInitialTimelineBuffer();

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
        initialBottomPinPending = activeRoomId.length > 0;
        initialTimelineBufferPending = activeRoomId.length > 0;
        lastInitialBufferTriggerCount = -1;
        if (!matrixTimelineList)
            return;

        matrixTimelineList.keepPinnedToBottom = true;
        matrixTimelineList.previousCount = 0;
    }

    onLoadingChanged: {
        if (!loading) {
            ensureInitialBottomPin();
            maybeRequestInitialTimelineBuffer();
        }
    }
}
