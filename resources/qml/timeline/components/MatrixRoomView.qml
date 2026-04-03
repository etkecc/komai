// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../styles/bubble"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import cc.etke.komai

ColumnLayout {
    id: root

    required property var roomPreview
    required property var dialogSupport
    required property var messageActionsRoomModel
    required property var composerInputController
    required property var externalDialogHost
    required property var externalHeaderPane
    required property var externalComposerPane
    property bool walkModeActive: false
    property bool roomSwitchInProgress: false
    property string focusedEventId: ""
    property string highlightedEventId: ""
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

    readonly property bool hasTimeline: TimelineManager.matrixTimelineItemCount > 0
    readonly property bool loading: TimelineManager.matrixTimelineLoading
    readonly property bool perfDisableComposer: TimelineManager.perfUiFlagEnabled("disable_composer")
    readonly property bool perfDisableTimelineBubbles: TimelineManager.perfUiFlagEnabled("disable_timeline_bubbles")
    readonly property bool perfMinimalTextBubbles: TimelineManager.perfUiFlagEnabled("minimal_text_bubbles")
    readonly property int composerBaselineHeight: Math.max(48, Komai.navigationRowHeight)
    readonly property var composerShell: externalComposerPane.composerShell
    readonly property var notificationAreaItem: timelineViewport
    readonly property var timelineListItem: matrixTimelineList
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
    property bool restoringEditDraft: false
    property int lastPaginationTriggerCount: -1
    property int lastInitialBufferTriggerCount: -1
    property string activeRoomId: roomPreview ? String(roomPreview.roomid || "") : ""
    property var measuredTimelineHeights: ({})
    property bool initialBottomPinPending: false
    property bool initialTimelineBufferPending: false
    property bool deferredInitialBufferTopUpPending: false
    property bool bufferPaginationInFlight: false
    property bool initialBufferCheckQueued: false
    property bool deferredBufferCheckQueued: false
    property int initialBufferCheckGeneration: 0
    property int deferredBufferCheckGeneration: 0
    property bool perfLoggedCountNonZero: false
    property bool perfLoggedContentHeightReady: false
    property bool perfLoggedUsefulHeightReady: false
    property bool perfLoggedBufferFilled: false
    property bool suppressNextWalkModeOlderStep: false
    property string lastMarkedReadEventId: ""
    property bool preferLatestReadMarkerEvent: false
    property int readMarkerGeneration: 0
    property bool pendingComposerAutoFocus: false
    property int _composerAutoFocusRetries: 0


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

    MatrixRoomLifecycleSupport {
        id: lifecycleSupport

        rootItem: root
        topBar: externalHeaderPane.headerItem
        listShellSupport: listShellSupport
        viewportSupport: viewportSupport
    }

    function clearSearch() { return lifecycleSupport.clearSearch(); }
    function markRoomSwitchPerfPhase(phase) { return lifecycleSupport.markRoomSwitchPerfPhase(phase); }

    function selectedEventIdsContains(eventId) { return eventSupport.selectedEventIdsContains(eventId); }
    function canExplicitlySelectEventId(eventId) { return eventSupport.canExplicitlySelectEventId(eventId); }
    function updateSelectionAnchor(preferredEventId) { return eventSupport.updateSelectionAnchor(preferredEventId); }
    function toggleSelectionForEventId(eventId) { return eventSupport.toggleSelectionForEventId(eventId); }
    function registerVisibleDelegate(eventId, delegateItem) { return eventSupport.registerVisibleDelegate(eventId, delegateItem); }
    function unregisterVisibleDelegate(eventId, delegateItem) { return eventSupport.unregisterVisibleDelegate(eventId, delegateItem); }
    function replaceTrackedEventId(previousId, nextId) { return eventSupport.replaceTrackedEventId(previousId, nextId); }

    function scheduleInitialTimelineBufferCheck() { return viewportSupport.scheduleInitialTimelineBufferCheck(); }
    function scheduleDeferredInitialTimelineBufferCheck() { return viewportSupport.scheduleDeferredInitialTimelineBufferCheck(); }
    function scheduleReadMarkerUpdate(preferLatestEvent) { return viewportSupport.scheduleReadMarkerUpdate(preferLatestEvent); }
    function ensureInitialBottomPin() { return viewportSupport.ensureInitialBottomPin(); }
    function updatePreferredInitialTimelinePageSize() { return viewportSupport.updatePreferredInitialTimelinePageSize(); }
    function isEffectivelyAtLiveEdge() { return viewportSupport.isEffectivelyAtLiveEdge(); }
    function bottomMostVisibleEventId() { return viewportSupport.bottomMostVisibleEventId(); }
    function selectableEventIdNearMatrixRow(row) { return viewportSupport.selectableEventIdNearMatrixRow(row); }

    function clearSelectedEvents() { return walkModeSupport.clearSelectedEvents(); }
    function handleMouseSelectionToggle(eventId) { return walkModeSupport.handleMouseSelectionToggle(eventId); }
    function enterWalkModeFromBottomMostVisible() { return walkModeSupport.enterWalkModeFromBottomMostVisible(); }
    function enterWalkModeAndMoveTowardOlderEventsByChunk() { return walkModeSupport.enterWalkModeAndMoveTowardOlderEventsByChunk(); }
    function isSelectableMatrixTimelineRow(row) { return walkModeSupport.isSelectableMatrixTimelineRow(row); }
    function canPerformWalkModeAction(actionName) { return walkModeSupport.canPerformWalkModeAction(actionName); }
    function performWalkModeAction(actionName) { return walkModeSupport.performWalkModeAction(actionName); }
    function exitWalkMode(options) { return walkModeSupport.exitWalkMode(options); }
    function timelineSelectionFocusTarget() { return walkModeSupport.timelineSelectionFocusTarget(); }
    function focusTimelineSelection() { return walkModeSupport.focusTimelineSelection(); }
    function openWalkModeHelpDialog() { return walkModeSupport.openWalkModeHelpDialog(); }
    function openPrimaryMessageActionsDialog() { return walkModeSupport.openPrimaryMessageActionsDialog(); }

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
    function openRemoveMessageDialog(eventId) { return interactionSupport.openRemoveMessageDialog(eventId); }
    function openRawMessageDialog(eventId) { return interactionSupport.openRawMessageDialog(eventId); }
    function openReadReceiptsDialog(eventId) { return interactionSupport.openReadReceiptsDialog(eventId); }
    function openMatrixForwardDialog(eventId) { return interactionSupport.openMatrixForwardDialog(eventId); }
    function openForwardDialog(eventId) { return interactionSupport.openForwardDialog(eventId); }
    function openReportMessageDialog(eventId) { return interactionSupport.openReportMessageDialog(eventId); }
    function openMessageActionsDialog(eventId, threadId, eventType, isSender, isEncrypted, isEditable, link, text, messageModelOverride, roomModelOverride) {
        return interactionSupport.openMessageActionsDialog(eventId,
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

    enabled: visible
    spacing: 0
    visible: !!roomPreview && roomPreview.isMatrixSummary

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

                    function updateStableThumbSize() { return listShellSupport.updateStableThumbSize(); }
                    function updateLastScroll() { return listShellSupport.updateLastScroll(); }
                    function updateBottomPin() { return listShellSupport.updateBottomPin(); }
                    function maybeScrollToBottom(force) { return listShellSupport.maybeScrollToBottom(force); }

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
                    displayMarginBeginning: root.listViewDisplayMargin
                    displayMarginEnd: root.listViewDisplayMargin
                    cacheBuffer: root.listViewCacheBuffer
                    model: TimelineManager.matrixTimelineModel
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
                        required property string threadId
                        required property string body
                        required property string formattedBody
                        required property string formattedStateEvent
                        required property string stateEventIconSource
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
                        readonly property string stableEventId: eventId.length > 0 ? eventId : itemId

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
                        height: bubbleStyle.height > 0
                            ? bubbleStyle.height
                            : heuristicHeight

                        TimelineBubbleMessageStyle {
                            id: bubbleStyle

                            eventId: timelineItemDelegate.stableEventId
                            replyTo: timelineItemDelegate.replyTo
                            room: TimelineManager.matrixTimelineModel
                            index: timelineItemDelegate.index
                            day: timelineItemDelegate.day
                            isSender: timelineItemDelegate.isSender
                            isStateEvent: timelineItemDelegate.isStateEvent
                            timestamp: timelineItemDelegate.timestamp
                            userId: timelineItemDelegate.userId
                            userName: timelineItemDelegate.userName
                            threadId: timelineItemDelegate.threadId
                            userPowerlevel: 0
                            isEdited: timelineItemDelegate.isEdited
                            isEncrypted: timelineItemDelegate.isEncrypted
                            reactions: timelineItemDelegate.reactions
                            status: timelineItemDelegate.status
                            trustlevel: 0
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
                }
                MatrixRoomEmptyState {
                    rootItem: root
                }
            }
        }
    }

}
