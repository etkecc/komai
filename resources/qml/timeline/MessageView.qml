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
    property string searchString: ""
    property bool filterByNotifications: false
    property bool disableTimelineList: false
    property bool suppressRoomSwitchSpinner: false
    readonly property bool filteringInProgress: filteredTimeline.filteringInProgress
    readonly property bool filteringRequested: searchString.length > 0 || filterByNotifications || (activeRoomModel && activeRoomModel.thread !== "")
    property bool perfFirstVisibleItemLogged: false
    property Room roommodel: room
    property var activeRoomModel: null
    property var pendingRoomModel: null
    property bool roomSwitchInProgress: false
    property int roomSwitchBindSerial: 0
    property string selectedEventId: ""
    property bool keyboardActionsOpen: false
    property var visibleTimelineDelegates: ({})
    property string pendingKeyboardActionsEventId: ""
    readonly property bool hasSelectedEvent: selectedEventId.length > 0
    readonly property real listViewDisplayMargin: roomSwitchInProgress ? 0 : chat.height / 8
    readonly property real listViewCacheBuffer: roomSwitchInProgress ? 0 : 320

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

    function clearSelectedEvent() {
        pendingKeyboardActionsEventId = "";

        if (typeof messageActionsHost !== "undefined"
                && messageActionsHost
                && messageActionsHost.control
                && messageActionsHost.control.keyboardActive)
            messageActionsHost.control.dismiss();

        selectedEventId = "";
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

    function selectedVisibleIndex() {
        if (!selectedEventId || !chat.model)
            return -1;

        for (let index = 0; index < chat.count; index++) {
            if (displayedEventIdAt(index) === selectedEventId)
                return index;
        }

        return -1;
    }

    function selectedDelegate() {
        if (!selectedEventId)
            return null;

        return visibleTimelineDelegates[selectedEventId] || null;
    }

    function bottomMostVisibleDelegate() {
        let bestDelegate = null;
        let bestBottom = -1;
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

            if (!bestDelegate || delegateBottom > bestBottom) {
                bestDelegate = delegate;
                bestBottom = delegateBottom;
            }
        }

        return bestDelegate;
    }

    function scrollDisplayedIndexIntoView(index) {
        if (index < 0)
            return;

        chat.keepPinnedToBottom = false;
        chat.positionViewAtIndex(index, ListView.Visible);
        chat.updateLastScroll();
    }

    function selectedMessageInfo() {
        const index = selectedVisibleIndex();
        if (index < 0 || !chat.model || typeof chat.model.dataByIndex !== "function")
            return null;

        function roleValue(role, fallbackValue) {
            const value = chat.model.dataByIndex(index, role);
            return value === undefined || value === null ? fallbackValue : value;
        }

        return {
            "eventId": displayedEventIdAt(index),
            "threadId": String(roleValue(Room.ThreadId, "") || ""),
            "type": Number(roleValue(Room.Type, -1)),
            "isSender": !!roleValue(Room.IsSender, false),
            "isEncrypted": !!roleValue(Room.IsEncrypted, false),
            "isEditable": !!roleValue(Room.IsEditable, false),
            "isStateEvent": !!roleValue(Room.IsStateEvent, false),
            "body": String(roleValue(Room.Body, "") || "")
        };
    }

    function validateSelectedEvent() {
        if (!selectedEventId)
            return false;

        if (selectedVisibleIndex() < 0) {
            clearSelectedEvent();
            return false;
        }

        return true;
    }

    function moveSelection(delta) {
        const currentIndex = selectedVisibleIndex();
        if (currentIndex < 0)
            return false;

        const step = delta >= 0 ? 1 : -1;
        for (let nextIndex = currentIndex + step; nextIndex >= 0 && nextIndex < chat.count; nextIndex += step) {
            if (displayedEventHiddenAt(nextIndex))
                continue;

            const nextEventId = displayedEventIdAt(nextIndex);
            if (!nextEventId)
                continue;

            if (messageActionsHost.control.keyboardActive)
                messageActionsHost.control.dismiss();

            pendingKeyboardActionsEventId = "";
            selectedEventId = nextEventId;
            focusTimelineSelection();
            scrollDisplayedIndexIntoView(nextIndex);
            return true;
        }

        return false;
    }

    function openKeyboardActionsForSelection() {
        if (!validateSelectedEvent())
            return false;

        focusTimelineSelection();

        const delegate = selectedDelegate();
        if (!delegate) {
            pendingKeyboardActionsEventId = selectedEventId;
            scrollDisplayedIndexIntoView(selectedVisibleIndex());
            return false;
        }

        pendingKeyboardActionsEventId = "";
        delegate.openKeyboardMessageActions();
        Qt.callLater(function () {
            messageActionsHost.control.focusFirstVisibleButton();
        });
        return true;
    }

    function closeKeyboardActions() {
        pendingKeyboardActionsEventId = "";
        if (!messageActionsHost.control.keyboardActive)
            return false;

        messageActionsHost.control.dismiss();
        focusTimelineSelection();
        return true;
    }

    function moveKeyboardActionsFocus(step) {
        if (!messageActionsHost.control.keyboardActive)
            return false;

        return messageActionsHost.control.moveFocus(step);
    }

    function tryOpenPendingKeyboardActions() {
        if (!pendingKeyboardActionsEventId || pendingKeyboardActionsEventId !== selectedEventId)
            return false;

        if (!selectedDelegate())
            return false;

        return openKeyboardActionsForSelection();
    }

    function selectBottomMostVisibleEvent() {
        const delegate = bottomMostVisibleDelegate();
        if (!delegate || !delegate.eventId)
            return false;

        pendingKeyboardActionsEventId = "";
        selectedEventId = String(delegate.eventId);
        focusTimelineSelection();
        return true;
    }

    function performSelectedMessageAction(actionName) {
        const message = selectedMessageInfo();
        if (!message)
            return false;

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
        default:
            handled = false;
        }

        if (handled && messageActionsHost.control.keyboardActive)
            messageActionsHost.control.dismiss();

        return handled;
    }

    function openSelectedMessageActionsDialog() {
        const message = selectedMessageInfo();
        if (!message)
            return false;

        if (messageActionsHost.control.keyboardActive)
            messageActionsHost.control.dismiss();

        return messageActionSupport.openOptionsDialog(chatRoot, message);
    }

    function scheduleTimelineModelBinding() {
        roomSwitchBindSerial += 1;
        const targetRoom = roommodel;

        if (!targetRoom || disableTimelineList) {
            clearSelectedEvent();
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
        clearSelectedEvent();
        resetVisibleDelegateRegistry();
        scheduleTimelineModelBinding();
    }
    onDisableTimelineListChanged: scheduleTimelineModelBinding()

    Component.onCompleted: scheduleTimelineModelBinding()

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

    ListView {
        id: chat

        property int delegateMaxWidth: ((Settings.uiLayoutContentMaxWidthEffectivePx > 0 && Settings.uiLayoutContentMaxWidthEffectivePx < chatRoot.availableWidth) ? Settings.uiLayoutContentMaxWidthEffectivePx : chatRoot.availableWidth) - chatRoot.padding * 2 - (scrollbar.interactive ? scrollbar.width : 0)

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
                chatRoot.clearSelectedEvent();
                chatRoot.resetVisibleDelegateRegistry();
                return;
            }
            Qt.callLater(function () {
                chatRoot.validateSelectedEvent();
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
                chatRoot.validateSelectedEvent();
                chatRoot.tryOpenPendingKeyboardActions();
            }

            function onRowsRemoved() {
                chatRoot.validateSelectedEvent();
            }

            function onLayoutChanged() {
                chatRoot.validateSelectedEvent();
                chatRoot.tryOpenPendingKeyboardActions();
            }

            function onModelReset() {
                chatRoot.clearSelectedEvent();
                chatRoot.resetVisibleDelegateRegistry();
            }

            target: chat.model
            ignoreUnknownSignals: true
        }
        Connections {
            function onActivationModeChanged() {
                chatRoot.keyboardActionsOpen = messageActionsHost.control.keyboardActive;
            }

            target: messageActionsHost.control
        }
        Connections {
            function onEventIdReplaced(oldId, newId) {
                if (chatRoot.selectedEventId === oldId)
                    chatRoot.selectedEventId = newId;
                if (chatRoot.pendingKeyboardActionsEventId === oldId)
                    chatRoot.pendingKeyboardActionsEventId = newId;
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

            prompt: qsTr("Enter reason for removal or hit enter for no reason:")
            title: qsTr("Reason for removal")
            titleIcon: ":/icons/icons/ui/delete.svg"
            acceptText: qsTr("Remove")

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
