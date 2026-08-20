// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../composer" as Composer
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
    property bool poolActive: true
    property var perRoomModel: null
    required property var dialogSupport
    required property var messageActionsRoomModel
    required property var composerInputController
    required property var externalDialogHost
    required property var externalHeaderPane
    required property var externalComposerPane
    property var composerRoom: null
    property bool walkModeActive: false
    property bool roomSwitchInProgress: false
    property string focusedEventId: ""
    property string highlightedEventId: ""
    // The last event-jump target. Outlives the transient highlight flash:
    // model resets from trailing pagination/receipt snapshots keep
    // re-anchoring the viewport on this event until the user takes over
    // scrolling (wheel, scrollbar drag, walk mode) or switches rooms.
    property string jumpAnchorEventId: ""
    property var selectedEventIds: []
    property string selectionAnchorEventId: ""
    property var visibleTimelineDelegates: ({})
    property int delegateRegistrationGeneration: 0
    property int delegateRegistrationRevision: 0
    readonly property int selectedCount: selectedEventIds.length
    readonly property bool hasSelectedEvents: selectedCount > 0
    readonly property bool hasSingleSelection: selectedCount === 1
    readonly property string singleSelectedEventId: hasSingleSelection ? String(selectedEventIds[0] || "") : ""
    readonly property string primaryActionEventId: hasSingleSelection
        ? singleSelectedEventId
        : (!hasSelectedEvents ? focusedEventId : "")
    readonly property bool hasFocusedEvent: focusedEventId.length > 0
    readonly property bool selectionModeCopyShortcutEnabled: {
        if (!(walkModeActive || hasSelectedEvents || hasFocusedEvent))
            return false;

        const activeItem = root.Window.activeFocusItem;
        return (itemIsInSubtree(activeItem, root)
                || itemIsInSubtree(activeItem, externalComposerPane)
                || itemIsInSubtree(activeItem, externalHeaderPane ? externalHeaderPane.headerItem : null)
                || itemIsInSubtree(activeItem, dialogSupport ? dialogSupport.messageActionsHost : null))
            && !focusOwnsSelectedTextCopy(activeItem);
    }

    readonly property bool hasTimeline: {
        if (threadViewActive)
            return threadTimelineModel ? threadTimelineModel.count > 0 : false;
        return perRoomModel ? perRoomModel.count > 0 : false;
    }
    readonly property bool loading: TimelineManager.matrixTimelineLoading
    readonly property bool perfDisableComposer: TimelineManager.perfUiFlagEnabled("disable_composer")
    readonly property bool perfDisableTimelineBubbles: TimelineManager.perfUiFlagEnabled("disable_timeline_bubbles")
    readonly property bool perfMinimalTextBubbles: TimelineManager.perfUiFlagEnabled("minimal_text_bubbles")
    readonly property bool perfDisableDelegateReuse: TimelineManager.perfUiFlagEnabled("disable_delegate_reuse")
    readonly property int composerBaselineHeight: Math.max(48, Komai.navigationRowHeight)
    readonly property var composerShell: externalComposerPane.composerShell
    readonly property var notificationAreaItem: timelineViewport
    readonly property var timelineListItem: matrixTimelineList
    readonly property alias filteredTimeline: filteredTimeline
    readonly property bool headerSearchHasFocus: !!externalHeaderPane.searchHasFocus
    readonly property real listViewDisplayMargin: roomSwitchInProgress
        ? 0
        : (matrixTimelineList ? matrixTimelineList.height / 8 : 0)
    readonly property real listViewCacheBuffer: roomSwitchInProgress ? 0 : 320
    readonly property int pendingAttachmentCount: TimelineManager.matrixTimelineAttachmentCount
    readonly property bool hasPendingAttachments: pendingAttachmentCount > 0
    readonly property string activeEditEventId: TimelineManager.matrixTimelineEditEventId
    readonly property bool editing: activeEditEventId.length > 0
    property string draftBeforeEdit: ""
    property int openOverlayDialogCount: 0
    readonly property bool hasOpenOverlayDialog: openOverlayDialogCount > 0
    readonly property string searchString: externalHeaderPane.headerItem
        ? String(externalHeaderPane.headerItem.searchString || "") : ""
    readonly property bool filteringRequested: searchString.length > 0
    readonly property bool filteringInProgress: filteredTimeline.filteringInProgress
    property bool restoringEditDraft: false
    property int lastPaginationTriggerCount: -1
    property int lastInitialBufferTriggerCount: -1
    property int lastInitialBufferTriggerRawCount: -1
    property string activeRoomId: ""
    property var measuredTimelineHeights: ({})
    property bool initialBottomPinPending: false
    property bool initialTimelineBufferPending: false
    property bool deferredInitialBufferTopUpPending: false
    // Counts initial-buffer paginate attempts where the visible count
    // did not advance (typical when raw items are arriving but every one
    // is filtered out by the user's hidden-event preferences). Capped to
    // avoid runaway pagination in pathological rooms (e.g. a bot that
    // emitted thousands of membership changes). Reset to 0 when the
    // visible count advances.
    property int paginationProgresslessAttempts: 0
    property bool bufferPaginationInFlight: false
    property bool initialBufferCheckQueued: false
    property bool deferredBufferCheckQueued: false
    property bool perfLoggedCountNonZero: false
    property bool perfLoggedContentHeightReady: false
    property bool perfLoggedUsefulHeightReady: false
    property bool perfLoggedBufferFilled: false
    property bool suppressNextWalkModeOlderStep: false
    property string lastMarkedReadEventId: ""
    property bool preferLatestReadMarkerEvent: false
    property int readMarkerGeneration: 0
    // Exposed for MatrixRoomViewportSupport's read-marker gate; the
    // support is a QtObject without its own Window attached property,
    // so the focus check is mirrored here where `Window.active` is
    // resolvable through the Item ancestry.
    readonly property bool windowActive: Window.active
    property bool pendingComposerAutoFocus: false
    property int _composerAutoFocusRetries: 0
    // Sticky thread state: only re-synced from TimelineManager while this
    // entry is the foreground pool slot. Background entries don't react to
    // the foreground tab's thread open/close, which avoids two problems:
    //   1. The QV4 crash from a global delegate-rebuild storm when every
    //      cached MatrixRoomView in the pool rebuilds simultaneously.
    //   2. Spurious ListView model swaps on tab switch driven by the
    //      pool-flip race: the incoming entry's bindings would activate
    //      against stale global state (the outgoing tab's thread) before
    //      C++ catches up, causing 2-3 delegate-pool flushes per switch.
    //
    // On pool reactivation the sync runs via Qt.callLater so that C++'s
    // matrixTimelineStateChanged / matrixThreadTimelineChanged emits land
    // first; the incoming entry then sees only the correct, post-switch
    // state and rebuilds at most once.
    property bool threadViewActive: false
    property var threadTimelineModel: null
    property bool threadTimelineLoading: false

    function _syncThreadFromManager() {
        threadViewActive = TimelineManager.matrixTimelineThreadEventId.length > 0;
        threadTimelineModel = TimelineManager.matrixThreadTimelineModel;
        threadTimelineLoading = TimelineManager.matrixThreadTimelineLoading;
    }

    Connections {
        target: TimelineManager
        enabled: root.poolActive
        function onMatrixThreadTimelineChanged() { root._syncThreadFromManager(); }
    }

    onPoolActiveChanged: {
        if (poolActive) {
            // Defer until the post-pool-flip turn of the event loop so
            // C++ has a chance to emit its tab-switch signals first;
            // otherwise we'd sync to the outgoing tab's stale thread
            // state and then have to swap again.
            Qt.callLater(_syncThreadFromManager);
        }
    }

    Component.onCompleted: {
        if (poolActive)
            _syncThreadFromManager();
    }
    // The MatrixTimelineModel the selection / walk-mode / visible-row helpers
    // must operate on: the thread timeline while a thread is open, the room
    // timeline otherwise. In the room case the ListView is sometimes bound to
    // `filteredTimeline` instead of `perRoomModel` (search / collapse-thread-
    // replies), and the two proxies have different row spaces — proxy row R is
    // typically a different event than source row R. Selection code must run
    // against whichever model is on screen, otherwise `ListView.indexAt(...)`
    // (which returns proxy rows) feeds row numbers that resolve to the wrong
    // events when looked up against the source (drag-select silently skipped
    // every "row" it touched when the cursor crossed a collapsed thread root).
    //
    // Both `MatrixTimelineModel` and `TimelineFilter` expose the same QML API
    // (rowForEventId / itemAt / count / dataByIndex), so callers can treat
    // this opaquely. Inside a thread view no filtering applies.
    readonly property var activeTimelineModel: threadViewActive
        ? threadTimelineModel
        : ((filteringRequested || filteredTimeline.collapseThreadReplies)
            ? filteredTimeline
            : perRoomModel)
    property int _collapseByRoomRevision: 0


    MessageActionSupport {
        id: messageActionSupport
    }

    MatrixRoomWalkModeSupport {
        id: walkModeSupport

        rootItem: root
        timelineList: matrixTimelineList
        topBar: externalHeaderPane.headerItem
        dialogHost: externalDialogHost
        messageActionSupport: messageActionSupport
    }

    MatrixRoomInteractionSupport {
        id: interactionSupport

        rootItem: root
        composerPane: externalComposerPane
        dialogSupport: root.dialogSupport
        composerInputController: root.composerInputController
        timelineList: matrixTimelineList
    }

    MatrixRoomViewportSupport {
        id: viewportSupport

        rootItem: root
        timelineList: matrixTimelineList
    }

    MatrixRoomListShellSupport {
        id: listShellSupport

        rootItem: root
        timelineList: matrixTimelineList
        scrollbar: matrixTimelineScrollbar
    }

    MatrixRoomEventSupport {
        id: eventSupport

        rootItem: root
    }

    TimelineFilter {
        id: filteredTimeline

        source: root.perRoomModel
        filterByContent: root.searchString
        collapseThreadReplies: {
            let _rev = root._collapseByRoomRevision;
            return Settings.resolvedTimelineThreadsCollapseReplies(root.activeRoomId);
        }

        onRequestMoreData: TimelineManager.paginateActiveMatrixTimelineBackwards(50)

        onCollapseThreadRepliesChanged: {
            Qt.callLater(function() {
                if (matrixTimelineList) {
                    matrixTimelineList.forceLayout();
                    matrixTimelineList.maybeScrollToBottom(true);
                }
            });
        }

        // The proxy resets itself on every committed query change
        // (startFiltering) and on thread-collapse toggles. When the
        // ListView is bound to the proxy at that moment (query edits, or
        // any search while collapseThreadReplies keeps the proxy bound),
        // the reset releases every visible delegate into the reuse pool
        // and rebuilds from it, and Qt 6 can hand back a pooled delegate
        // still bound to its previous row (see flushDelegateReusePool),
        // which surfaces as overlapping messages (#181). Flush before the
        // reset so the rebuild starts from a clean pool. Also covers
        // source-model resets propagating through the proxy, which the
        // perRoomModel-targeted flush hooks don't reach while the proxy is
        // the bound model.
        onModelAboutToBeReset: {
            if (matrixTimelineList.model === filteredTimeline)
                listShellSupport.flushDelegateReusePool();
        }
    }

    // When thread collapse is active, paginated items may all be thread
    // replies (filtered out).  The ListView's countChanged won't fire,
    // stalling the buffer-fill loop.  Watch the raw model's count to
    // unblock pagination in that case.
    Connections {
        target: root.perRoomModel
        enabled: filteredTimeline.collapseThreadReplies && !root.threadViewActive
        function onCountChanged() {
            if (root.bufferPaginationInFlight) {
                root.bufferPaginationInFlight = false;
                if (root.deferredInitialBufferTopUpPending)
                    root.scheduleDeferredInitialTimelineBufferCheck();
                else if (root.initialTimelineBufferPending)
                    root.scheduleInitialTimelineBufferCheck();
            }
        }
    }

    // Symmetric to the block above, but at a different layer: the model's
    // *raw* count (allItems_) can grow without the visible count changing
    // when every paginated event is filtered out by hidden-event prefs.
    // Without this nudge the initial-buffer loop has nothing to wake it up
    // and we'd be stuck on "No messages to display".
    Connections {
        target: root.perRoomModel
        enabled: !root.threadViewActive
        function onRawCountChanged() {
            if (root.bufferPaginationInFlight) {
                root.bufferPaginationInFlight = false;
                if (root.deferredInitialBufferTopUpPending)
                    root.scheduleDeferredInitialTimelineBufferCheck();
                else if (root.initialTimelineBufferPending)
                    root.scheduleInitialTimelineBufferCheck();
            }
        }
    }

    // Thread view pagination: when the list reaches the oldest item
    // (atYEnd in BottomToTop mode = visual top), load more thread events.
    //
    // Gated on `poolActive`, but binding-evaluation timing during pool
    // deactivation can let a stray `atYEnd` change race past the gate
    // (the ListView re-layouts as the parent ColumnLayout becomes invisible,
    // and Qt's binding propagation order isn't guaranteed to mark this
    // Connection disabled before that geometry signal fires). The C++ side
    // also gates on `expectedRoomId` matching the active room as a belt-
    // and-braces backstop.
    Connections {
        target: matrixTimelineList
        enabled: root.threadViewActive && root.poolActive
        function onAtYEndChanged() {
            // `activeRoomId` is empty during the cold-path window between
            // a fresh component being created and the pool assigning its
            // room. Without this guard, a transient `atYEnd=true` from the
            // empty ListView's initial layout calls into C++ with an empty
            // expected room id, which the C++ defensive check treats as
            // "no expectation" and lets through — leaking pagination to
            // whichever thread the Rust runtime still considers active.
            if (matrixTimelineList.atYEnd && root.threadViewActive
                    && root.poolActive
                    && !root.threadTimelineLoading
                    && root.activeRoomId.length > 0) {
                TimelineManager.paginateActiveMatrixThreadTimelineBackwards(
                    50, root.activeRoomId);
            }
        }
    }

    Connections {
        target: Settings
        function onTimelineThreadsCollapseRepliesByRoomChanged() {
            root._collapseByRoomRevision++;
        }
        function onTimelineThreadsCollapseRepliesChanged() {
            root._collapseByRoomRevision++;
        }
    }

    Connections {
        target: root.perRoomModel
        function onAboutToReplaceContent() { listShellSupport.handleModelResetAboutToReplace(); }
        function onContentReplaced() { listShellSupport.handleModelResetContentReplaced(); }
    }

    Binding {
        target: externalHeaderPane.headerItem
        property: "filteringInProgress"
        value: root.filteringInProgress
        when: !!externalHeaderPane.headerItem
    }

    MatrixRoomLifecycleSupport {
        id: lifecycleSupport

        rootItem: root
        topBar: externalHeaderPane.headerItem
        listShellSupport: listShellSupport
        viewportSupport: viewportSupport
    }

    function clearSearch() { return lifecycleSupport.clearSearch(); }
    function markRoomSwitchPerfPhase(phase) { return lifecycleSupport.markRoomSwitchPerfPhase(phase); }
    function handlePoolReactivation(preserveScroll) { return lifecycleSupport.handlePoolReactivation(preserveScroll); }

    function selectedEventIdsContains(eventId) { return eventSupport.selectedEventIdsContains(eventId); }
    function canExplicitlySelectEventId(eventId) { return eventSupport.canExplicitlySelectEventId(eventId); }
    function updateSelectionAnchor(preferredEventId) { return eventSupport.updateSelectionAnchor(preferredEventId); }
    function toggleSelectionForEventId(eventId) { return eventSupport.toggleSelectionForEventId(eventId); }
    function selectRangeToEventId(eventId) { return eventSupport.selectRangeToEventId(eventId); }
    function registerVisibleDelegate(eventId, delegateItem) { return eventSupport.registerVisibleDelegate(eventId, delegateItem); }
    function unregisterVisibleDelegate(eventId, delegateItem) { return eventSupport.unregisterVisibleDelegate(eventId, delegateItem); }
    function replaceTrackedEventId(previousId, nextId) { return eventSupport.replaceTrackedEventId(previousId, nextId); }

    function scheduleInitialTimelineBufferCheck() { return viewportSupport.scheduleInitialTimelineBufferCheck(); }
    function scheduleDeferredInitialTimelineBufferCheck() { return viewportSupport.scheduleDeferredInitialTimelineBufferCheck(); }
    function scheduleReadMarkerUpdate(preferLatestEvent) { return viewportSupport.scheduleReadMarkerUpdate(preferLatestEvent); }
    function ensureInitialBottomPin() { return viewportSupport.ensureInitialBottomPin(); }
    function updatePreferredInitialTimelinePageSize() { return viewportSupport.updatePreferredInitialTimelinePageSize(); }
    function isEffectivelyAtLiveEdge() { return viewportSupport.isEffectivelyAtLiveEdge(); }
    function isNearLiveEdge() { return viewportSupport.isNearLiveEdge(); }
    function bottomMostVisibleEventId() { return viewportSupport.bottomMostVisibleEventId(); }
    function selectableEventIdNearMatrixRow(row) { return viewportSupport.selectableEventIdNearMatrixRow(row); }

    function clearSelectedEvents() { return walkModeSupport.clearSelectedEvents(); }
    function handleMouseSelectionToggle(eventId) { return walkModeSupport.handleMouseSelectionToggle(eventId); }
    function handleMouseSelectionRangeTo(eventId) { return walkModeSupport.handleMouseSelectionRangeTo(eventId); }
    function dragSelectGestureBegan(eventId, modifiers) { return walkModeSupport.dragSelectGestureBegan(eventId, modifiers); }
    function dragSelectGestureMoved(eventId, scenePos, sourceItem) { return walkModeSupport.dragSelectGestureMoved(eventId, scenePos, sourceItem); }
    function dragSelectGestureEnded() { return walkModeSupport.dragSelectGestureEnded(); }
    function enterWalkModeFromBottomMostVisible() { return walkModeSupport.enterWalkModeFromBottomMostVisible(); }
    function enterWalkModeAndMoveTowardOlderEventsByChunk() { return walkModeSupport.enterWalkModeAndMoveTowardOlderEventsByChunk(); }
    function isSelectableMatrixTimelineRow(row) { return walkModeSupport.isSelectableMatrixTimelineRow(row); }
    function canPerformWalkModeAction(actionName) { return walkModeSupport.canPerformWalkModeAction(actionName); }
    function performWalkModeAction(actionName) { return walkModeSupport.performWalkModeAction(actionName); }
    function exitWalkMode(options) { return walkModeSupport.exitWalkMode(options); }
    function timelineSelectionFocusTarget() { return walkModeSupport.timelineSelectionFocusTarget(); }
    function focusTimelineSelection() { return walkModeSupport.focusTimelineSelection(); }
    function openWalkModeHelpDialog() { return walkModeSupport.openWalkModeHelpDialog(); }
    function lastRoomHeaderActionButtonTarget() { return walkModeSupport.lastRoomHeaderActionButtonTarget(); }
    function openPrimaryMessageActionsDialog() { return walkModeSupport.openPrimaryMessageActionsDialog(); }
    function copySelectionModeText(plainText) { return walkModeSupport.copySelectionModeText(plainText); }

    function matrixTimelineHeightCacheKey(eventId, itemId) { return eventSupport.matrixTimelineHeightCacheKey(eventId, itemId); }
    function rememberedTimelineHeight(cacheKey) { return eventSupport.rememberedTimelineHeight(cacheKey); }
    function rememberTimelineHeight(cacheKey, height) { return eventSupport.rememberTimelineHeight(cacheKey, height); }
    function matrixEventTypeForItemKind(kind) { return eventSupport.matrixEventTypeForItemKind(kind); }
    function matrixTimelineDayKey(timestampMs) { return eventSupport.matrixTimelineDayKey(timestampMs); }
    function isMatrixStateLikeKind(kind) { return eventSupport.isMatrixStateLikeKind(kind); }
    function formattedMatrixTextHtml(text) { return eventSupport.formattedMatrixTextHtml(text); }
    function matrixStateEventIconForKind(kind) { return eventSupport.matrixStateEventIconForKind(kind); }
    function matrixRedactedEventPair(senderDisplayName, senderId) { return eventSupport.matrixRedactedEventPair(senderDisplayName, senderId); }

    function openMatrixMessageContextMenu(messageModel, roomModel, copyText) {
        return interactionSupport.openMatrixMessageContextMenu(messageModel, roomModel, copyText);
    }
    function jumpToLoadedMatrixEvent(eventId) { return interactionSupport.jumpToLoadedMatrixEvent(eventId); }
    function focusTextInput() { return interactionSupport.focusTextInput(); }
    function destroyOnClose(dialog) { return interactionSupport.destroyOnClose(dialog); }
    function scheduleComposerAutoFocus() { return interactionSupport.scheduleComposerAutoFocus(); }
    function canHandleEscape() { return interactionSupport.canHandleEscape(); }
    function handleEscape() { return interactionSupport.handleEscape(); }
    function shouldRouteTextKeyToComposer(event) { return interactionSupport.shouldRouteTextKeyToComposer(event); }
    function handleComposerTextKey(event) { return interactionSupport.handleComposerTextKey(event); }
    function appendText(text) { return interactionSupport.appendText(text); }
    function trySendMessage() { return interactionSupport.trySendMessage(); }
    function beginEdit(eventId, body, messageKind) { return interactionSupport.beginEdit(eventId, body, messageKind); }
    function openRemoveMessageDialog(eventId, transactionId) { return interactionSupport.openRemoveMessageDialog(eventId, transactionId); }
    function openRawMessageDialog(eventId) { return interactionSupport.openRawMessageDialog(eventId); }
    function openReadReceiptsDialog(eventId) { return interactionSupport.openReadReceiptsDialog(eventId); }
    function openReactionDetailsDialog(eventId, reactions) { return interactionSupport.openReactionDetailsDialog(eventId, reactions); }
    function openMatrixForwardDialog(eventId) { return interactionSupport.openMatrixForwardDialog(eventId); }
    function openForwardDialog(eventId) { return interactionSupport.openForwardDialog(eventId); }
    function openForwardDialogForEvents(eventIds, selectionCount) { return interactionSupport.openForwardDialogForEvents(eventIds, selectionCount); }
    function openRemoveMessagesDialog(eventIds, selectionCount) { return interactionSupport.openRemoveMessagesDialog(eventIds, selectionCount); }
    function openReportMessageDialog(eventId) { return interactionSupport.openReportMessageDialog(eventId); }
    function openMessageActionsDialog(eventId, threadId, eventType, isSender, isEncrypted, isEditable, link, text, messageModelOverride, roomModelOverride, transactionId) {
        return interactionSupport.openMessageActionsDialog(eventId,
                                                           threadId,
                                                           eventType,
                                                           isSender,
                                                           isEncrypted,
                                                           isEditable,
                                                           link,
                                                           text,
                                                           messageModelOverride,
                                                           roomModelOverride,
                                                           transactionId);
    }

    function itemIsInSubtree(item, ancestor) {
        if (!item || !ancestor)
            return false;

        let current = item;
        while (current) {
            if (current === ancestor)
                return true;
            current = current.parent;
        }

        return false;
    }

    function focusOwnsSelectedTextCopy(item) {
        const activeItem = item || root.Window.activeFocusItem;
        if (!activeItem)
            return false;

        let current = activeItem;
        while (current) {
            const selectedText = current.selectedText;
            if (selectedText !== undefined && String(selectedText).length > 0)
                return true;

            if (current === root)
                break;

            current = current.parent;
        }

        return false;
    }

    enabled: visible
    spacing: 0
    visible: (!!roomPreview && roomPreview.isMatrixSummary) || (poolActive && activeRoomId.length > 0)

    Rectangle {
        Layout.fillHeight: true
        Layout.fillWidth: true

        // When viewing a thread, tint the timeline backdrop with the thread's
        // user color (same expression as ThreadViewBar) so the bar and the
        // timeline area read as a single coloured surface — a stronger
        // "you're in a thread" cue than the bar alone.
        readonly property color threadTintColor: root.threadViewActive
            ? TimelineManager.userColor(TimelineManager.matrixTimelineThreadEventId, palette.base)
            : palette.buttonText
        color: root.threadViewActive
            ? Qt.tint(palette.window, Qt.hsla(threadTintColor.hslHue, 0.7,
                                              threadTintColor.hslLightness, 0.1))
            : palette.base

        // Right-click on empty timeline space shows the settings shortcut.
        // Message bubbles and reply previews consume the right-click on press
        // (TimelineBubbleBody.qml), so the handler only fires on bare areas.
        TapHandler {
            acceptedButtons: Qt.RightButton
            onTapped: timelineSettingsMenu.popup()
        }

        Menu {
            id: timelineSettingsMenu

            Component.onCompleted: {
                if (timelineSettingsMenu.popupType != undefined)
                    timelineSettingsMenu.popupType = 2;
            }

            MenuItem {
                text: qsTr("Settings...") // Keep short: Qt may clip/elide longer menu item text
                icon.source: "qrc:/icons/icons/ui/settings.svg"

                onTriggered: MainWindow.showUserSettingsPage(UserSettingsModel.TabTimeline)
            }
        }

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
                        if (pressed) {
                            positionOnPress = position;
                        } else {
                            positionOnPress = -1;
                            listShellSupport.handleScrollbarReleased();
                        }
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
                        listShellSupport.handleScrollbarPositionChanged();
                    }
                }

                TimelineToEndButton {
                    chatList: matrixTimelineList
                    scrollbarItem: matrixTimelineScrollbar
                    z: 20
                }

                ListView {
                    id: matrixTimelineList

                    // Scrollbar width is already reserved by anchors.rightMargin
                    // below; delegates fill the full ListView width.
                    property int delegateMaxWidth: width
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

                    function updateStableThumbSize() { return listShellSupport.updateStableThumbSize(); }
                    function updateLastScroll() { return listShellSupport.updateLastScroll(); }
                    function updateBottomPin() { return listShellSupport.updateBottomPin(); }
                    function maybeScrollToBottom(force) { return listShellSupport.maybeScrollToBottom(force); }
                    function isNearLiveEdge() { return viewportSupport.isNearLiveEdge(); }
                    function isEffectivelyAtLiveEdge() { return viewportSupport.isEffectivelyAtLiveEdge(); }

                    anchors.fill: parent
                    anchors.margins: Komai.paddingMedium
                    anchors.bottomMargin: matrixTypingIndicator.visible ? 0 : Komai.paddingMedium
                    anchors.rightMargin: Komai.paddingMedium + (matrixTimelineScrollbar.interactive ? matrixTimelineScrollbar.width : 0)
                    // Komai is desktop-first: scrollbar + wheel + keyboard cover
                    // every scroll path. Disabling Flickable's own drag-scroll
                    // stops empty-space and image-bubble drags from getting
                    // hijacked into a scroll gesture (#124) and removes a
                    // touch-era affordance we don't otherwise lean on.
                    interactive: false
                    keyNavigationEnabled: false
                    KeyNavigation.priority: KeyNavigation.BeforeItem
                    Keys.priority: Keys.BeforeItem
                    clip: true
                    reuseItems: !root.perfDisableDelegateReuse
                    // Index 0 = newest (model is reversed in Rust).
                    // BottomToTop places index 0 at the visual bottom, matching
                    // chat convention (newest at bottom). This also makes
                    // contentHeight changes extend upward, away from the anchored
                    // viewport, which keeps the scrollbar thumb stable.
                    verticalLayoutDirection: ListView.BottomToTop
                    boundsBehavior: Flickable.StopAtBounds
                    displayMarginBeginning: root.listViewDisplayMargin
                    displayMarginEnd: root.listViewDisplayMargin
                    cacheBuffer: root.listViewCacheBuffer
                    model: root.threadViewActive
                        ? root.threadTimelineModel
                        : ((root.filteringRequested || filteredTimeline.collapseThreadReplies)
                            ? filteredTimeline : root.perRoomModel)
                    header: Item { width: 1; height: Komai.paddingSmall }
                    // BottomToTop layout: this footer is rendered at the visual
                    // top of the timeline.  It surfaces a spinner while the
                    // matrix-sdk task is fetching older history (e.g. when
                    // the user has scrolled to the top and pagination is
                    // hitting the server) — without this, scrolling at the
                    // top while history loads looks like the timeline has
                    // hung. Backed by perRoomModel.paginationInProgress.
                    footer: TimelineLoadingFooter {
                        delegateWidth: matrixTimelineList.delegateMaxWidth
                        // perRoomModel.paginationInProgress is room-scoped;
                        // thread view has its own pagination flow which we
                        // don't surface here.
                        roomModel: root.threadViewActive ? null : root.perRoomModel
                        filteringInProgress: root.filteringInProgress
                        searchString: root.searchString
                    }
                    spacing: Komai.paddingMedium
                    visible: root.hasTimeline

                    Keys.onPressed: event => {
                        if (walkModeSupport.handleWalkModeKey(event))
                            return;

                        root.handleComposerTextKey(event);
                    }
                    Keys.onShortcutOverride: event => {
                        if (event.key === Qt.Key_Escape
                                && (root.walkModeActive || root.hasSelectedEvents || root.hasFocusedEvent)) {
                            event.accepted = true;
                        }
                    }

                    WheelHandler {
                        orientation: Qt.Vertical
                        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad

                        onRotationChanged: {
                            listShellSupport.handleWheelRotation(rotation);
                        }
                    }

                    onMovementEnded: listShellSupport.handleMovementEnded()
                    onAtYBeginningChanged: listShellSupport.handleAtYBeginningChanged()
                    onContentYChanged: listShellSupport.handleContentYChanged()
                    onContentHeightChanged: listShellSupport.handleContentHeightChanged()
                    onHeightChanged: listShellSupport.handleHeightChanged()
                    onCountChanged: listShellSupport.handleCountChanged()
                    onModelChanged: listShellSupport.handleModelChanged()
                    Component.onCompleted: listShellSupport.handleCompleted()







                    delegate: Item {
                        id: timelineItemDelegate

                        property var chat: matrixTimelineList
                        property var chatRoot: root

                        // Model roles (canonical names from MatrixTimelineModel)
                        required property int index
                        required property string typeString
                        required property string eventId
                        required property string itemId
                        required property string transactionId
                        required property string sendError
                        required property bool isRecoverable
                        required property string threadId
                        required property bool isThreadRoot
                        required property int threadReplyCount
                        required property string body
                        required property string formattedBody
                        required property string formattedStateEvent
                        required property string stateEventIconSource
                        required property string stateEventIconColorCategory
                        required property var reactions
                        required property date timestamp
                        required property bool isEdited
                        required property int type
                        required property int day
                        required property bool isSender
                        required property string userId
                        required property string userName
                        required property bool isStateEvent
                        required property int status
                        required property bool isEncrypted
                        required property int messageShield
                        required property bool isEditable
                        required property bool isHiddenEvent
                        required property string replyTo
                        required property string url
                        required property string thumbnailUrl
                        required property int duration
                        required property string blurhash
                        required property string filename
                        required property string filesize
                        required property string mimetype
                        required property double proportionalHeight
                        required property string callType
                        required property string fileTypeIconSource
                        required property int originalWidth
                        required property int originalHeight
                        // Lookup key for content rendering (role-data lookup via
                        // EventDataSource.dataById). Falls back to `itemId` (matrix-sdk-ui
                        // `unique_id`) for local echoes that don't have a real event_id yet,
                        // so their bubble can still render. This is NOT a Matrix event id;
                        // action handlers must branch on `isLocalEcho` (keyed on
                        // `transactionId`, not on this lookup key) before passing it to
                        // any Rust handler that calls `EventId::parse`.
                        readonly property string stableEventId: eventId.length > 0 ? eventId : itemId
                        // Authoritative local-echo signal. matrix-sdk-ui only keeps
                        // `transaction_id` for pending/failed sends; it is cleared once the
                        // remote echo arrives, so its presence alone is reliable.
                        readonly property bool isLocalEcho: transactionId.length > 0

                        width: matrixTimelineList.width
                        readonly property real heuristicHeight: {
                            const lineH = Math.max(18, Math.round(Settings.uiFontSizePt * 1.8));
                            const pad = Komai.paddingMedium * 2;
                            if (isStateEvent)
                                return lineH + pad;
                            if (["image", "video", "sticker"].indexOf(typeString) >= 0) {
                                const aspect = originalWidth > 0 && originalHeight > 0
                                    ? originalHeight / originalWidth : 0.75;
                                return Math.min(300, Math.max(80, 200 * aspect)) + pad;
                            }
                            if (["file", "audio"].indexOf(typeString) >= 0)
                                return lineH * 3 + pad;
                            const lines = Math.max(1, Math.min(12, Math.ceil(String(body || "").length / 42)));
                            const replyH = replyTo.length > 0 ? lineH * 2 : 0;
                            return lines * lineH + replyH + pad + lineH;
                        }
                        height: sharedTimelineBubble.item
                            ? sharedTimelineBubble.item.height
                            : heuristicHeight

                        Component {
                            id: matrixPlainMessageStyle

                            TimelinePlainMessageStyle {
                                chatRoot: root
                                searchQuery: root.searchString

                                eventId: timelineItemDelegate.stableEventId
                                transactionId: timelineItemDelegate.transactionId
                                isLocalEcho: timelineItemDelegate.isLocalEcho
                                realEventId: timelineItemDelegate.eventId
                                sendError: timelineItemDelegate.sendError
                                isRecoverable: timelineItemDelegate.isRecoverable
                                replyTo: timelineItemDelegate.replyTo
                                // Must track the same model the ListView is bound to —
                                // EventDelegateChooser reads the inner delegate's content
                                // (formattedBody, type, …) via dataById/multiData on this
                                // model, and a mismatch between the row's identity and the
                                // source data causes stale/Unsupported flashes on local echo.
                                room: root.threadViewActive ? root.threadTimelineModel : root.perRoomModel
                                index: timelineItemDelegate.index
                                day: timelineItemDelegate.day
                                isSender: timelineItemDelegate.isSender
                                isStateEvent: timelineItemDelegate.isStateEvent
                                timestamp: timelineItemDelegate.timestamp
                                userId: timelineItemDelegate.userId
                                userName: timelineItemDelegate.userName
                                threadId: timelineItemDelegate.threadId
                                isThreadRoot: timelineItemDelegate.isThreadRoot
                                threadReplyCount: timelineItemDelegate.threadReplyCount
                                userPowerlevel: 0
                                isEdited: timelineItemDelegate.isEdited
                                isEncrypted: timelineItemDelegate.isEncrypted
                                reactions: timelineItemDelegate.reactions
                                status: timelineItemDelegate.status
                                trustlevel: 0
                                messageShield: timelineItemDelegate.messageShield
                                typeString: timelineItemDelegate.typeString
                                notificationlevel: MtxEvent.Empty
                                type: timelineItemDelegate.type
                                isEditable: timelineItemDelegate.isEditable
                                // Hidden rows are filtered out of MatrixTimelineModel
                                // before they reach the ListView, so delegate-level
                                // collapse is intentionally disabled here.
                                isHiddenEvent: false
                                messageContextMenu: dialogSupport.messageContextMenu
                                replyContextMenu: dialogSupport.replyContextMenu
                                messageActions: dialogSupport.messageActionsHost.control
                                roomAdapter: messageActionsRoomModel
                                scrolledToThis: root.highlightedEventId.length > 0
                                    && root.highlightedEventId === timelineItemDelegate.stableEventId
                            }
                        }

                        Component {
                            id: matrixBubbleMessageStyle

                            TimelineBubbleMessageStyle {
                                chatRoot: root
                                searchQuery: root.searchString

                                eventId: timelineItemDelegate.stableEventId
                                transactionId: timelineItemDelegate.transactionId
                                isLocalEcho: timelineItemDelegate.isLocalEcho
                                realEventId: timelineItemDelegate.eventId
                                sendError: timelineItemDelegate.sendError
                                isRecoverable: timelineItemDelegate.isRecoverable
                                replyTo: timelineItemDelegate.replyTo
                                room: root.threadViewActive ? root.threadTimelineModel : root.perRoomModel
                                index: timelineItemDelegate.index
                                day: timelineItemDelegate.day
                                isSender: timelineItemDelegate.isSender
                                isStateEvent: timelineItemDelegate.isStateEvent
                                timestamp: timelineItemDelegate.timestamp
                                userId: timelineItemDelegate.userId
                                userName: timelineItemDelegate.userName
                                threadId: timelineItemDelegate.threadId
                                isThreadRoot: timelineItemDelegate.isThreadRoot
                                threadReplyCount: timelineItemDelegate.threadReplyCount
                                userPowerlevel: 0
                                isEdited: timelineItemDelegate.isEdited
                                isEncrypted: timelineItemDelegate.isEncrypted
                                reactions: timelineItemDelegate.reactions
                                status: timelineItemDelegate.status
                                trustlevel: 0
                                messageShield: timelineItemDelegate.messageShield
                                typeString: timelineItemDelegate.typeString
                                notificationlevel: MtxEvent.Empty
                                type: timelineItemDelegate.type
                                isEditable: timelineItemDelegate.isEditable
                                // Hidden rows are filtered out of MatrixTimelineModel
                                // before they reach the ListView, so delegate-level
                                // collapse is intentionally disabled here.
                                isHiddenEvent: false
                                messageContextMenu: dialogSupport.messageContextMenu
                                replyContextMenu: dialogSupport.replyContextMenu
                                messageActions: dialogSupport.messageActionsHost.control
                                roomAdapter: messageActionsRoomModel
                                scrolledToThis: root.highlightedEventId.length > 0
                                    && root.highlightedEventId === timelineItemDelegate.stableEventId
                            }
                        }

                        Loader {
                            id: sharedTimelineBubble

                            active: true
                            sourceComponent: Settings.timelineMessagesStyle === Settings.TimelineMessagesStyle.Plain
                                ? matrixPlainMessageStyle
                                : matrixBubbleMessageStyle
                            visible: active
                        }
                    }
                }
                MatrixRoomEmptyState {
                    rootItem: root
                }
            }

            Composer.TypingIndicator {
                id: matrixTypingIndicator

                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingMedium
                Layout.rightMargin: Komai.paddingMedium
                room: root.composerRoom
                visible: Settings.timelineTypingShowEnabled
            }
        }
    }

}
