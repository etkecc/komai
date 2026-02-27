// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "./styles/bubble"
import "./styles/plain"
import "./components"
import "../ui"
import "../dialogs/navigation"
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.2
import QtQuick.Window 2.13
import im.nheko 1.0

Item {
    id: chatRoot

    property int availableWidth: width
    property int padding: Nheko.paddingMedium
    property string searchString: ""
    property bool filterByNotifications: false
    readonly property bool filteringInProgress: filteredTimeline.filteringInProgress
    property Room roommodel: room

    function openForwardDialog(eventId) {
        if (!eventId)
            return null;
        var forwardDialog = forwardCompleterComponent.createObject(timelineRoot);
        if (!forwardDialog)
            return null;
        forwardDialog.setMessageEventId(eventId);
        forwardDialog.open();
        timelineRoot.destroyOnClose(forwardDialog);
        return forwardDialog;
    }

    function showDialogFromComponent(componentRef, properties) {
        var dialog = componentRef.createObject(timelineRoot, properties || {});
        if (!dialog)
            return null;
        dialog.show();
        dialog.forceActiveFocus();
        timelineRoot.destroyOnClose(dialog);
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

        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.top: parent.top
        parent: chat.parent
    }
    ListView {
        id: chat

        property int delegateMaxWidth: ((Settings.uiLayoutContentMaxWidthEffectivePx > 0 && Settings.uiLayoutContentMaxWidthEffectivePx < chatRoot.availableWidth) ? Settings.uiLayoutContentMaxWidthEffectivePx : chatRoot.availableWidth) - chatRoot.padding * 2 - (scrollbar.interactive ? scrollbar.width : 0)

        ScrollBar.vertical: scrollbar
        anchors.fill: parent
        anchors.rightMargin: scrollbar.interactive ? scrollbar.width : 0
        // reuseItems still has a few bugs, see https://bugreports.qt.io/browse/QTBUG-95105 https://bugreports.qt.io/browse/QTBUG-95107
        //onModelChanged: if (room) room.sendReset()
        //reuseItems: true
        boundsBehavior: Flickable.StopAtBounds
        displayMarginBeginning: height / 8
        displayMarginEnd: height / 8
        model: (filteredTimeline.filterByThread || filteredTimeline.filterByContent || filteredTimeline.filterByNotifications) ? filteredTimeline : room
        //pixelAligned: true
        spacing: 2
        verticalLayoutDirection: ListView.BottomToTop

        property real lastScrollPos: 0
        property bool keepPinnedToBottom: true

        // Fixup the scroll position when the height changes. Without this, the view is kept around the center of the currently visible content, while we usually want to stick to the bottom.
        function updateLastScroll() {
            lastScrollPos = (contentY+height);
        }
        onMovementEnded: {
            updateLastScroll();
            keepPinnedToBottom = atYEnd;
        }
        onModelChanged: {
            updateLastScroll();
            keepPinnedToBottom = atYEnd;
        }
        onHeightChanged: {
            contentY = (lastScrollPos-height);
            if (keepPinnedToBottom)
                positionViewAtBeginning();
        }
        onContentHeightChanged: {
            if (keepPinnedToBottom && !moving && !flicking && !dragging) {
                positionViewAtBeginning();
                updateLastScroll();
            }
        }
        Component.onCompleted: {
            updateLastScroll();
            keepPinnedToBottom = atYEnd;
        }

        Component {
            id: plainMessageStyle

            TimelinePlainMessageStyle {
                messageActions: messageActionsC
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
                messageActions: messageActionsC
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
        footer: Item {
            width: chat.delegateMaxWidth
            // hacky, but works
            height: loadingSpinner.height + 2 * Nheko.paddingLarge

            // Hold spinner visible briefly after loading stops to prevent
            // flicker from rapid paginationInProgress toggles during search.
            property bool isLoading: ((room && room.paginationInProgress) || chatRoot.filteringInProgress) && !chatRoot.searchString
            visible: isLoading || spinnerHoldTimer.running
            onIsLoadingChanged: {
                if (isLoading)
                    spinnerHoldTimer.stop();
                else
                    spinnerHoldTimer.start();
            }
            Timer {
                id: spinnerHoldTimer
                interval: 200
            }

            Spinner {
                id: loadingSpinner

                anchors.centerIn: parent
                anchors.margins: Nheko.paddingLarge
                foreground: palette.mid
                running: parent.isLoading || spinnerHoldTimer.running
                z: 3
            }
        }

        Window.onActiveChanged: readTimer.running = Window.active
        onCountChanged: {
            // Mark timeline as read
            if (atYEnd && room)
                model.currentIndex = 0;
        }

        TimelineFilter {
            id: filteredTimeline

            filterByContent: chatRoot.searchString
            filterByNotifications: chatRoot.filterByNotifications
            filterByThread: room ? room.thread : ""
            source: room
        }
        // Click-outside overlay: dismisses the action bar when clicking
        // anywhere outside it.  Parented to chat.contentItem (same as
        // the action bar) so z-ordering works: the bar at z:10 renders
        // above the overlay at z:9, allowing button clicks and hovers
        // to reach the bar normally.  The overlay tracks the visible
        // viewport via chat.contentY / chat.width / chat.height.
        MouseArea {
            id: actionBarDismissOverlay
            parent: chat.contentItem
            x: 0
            y: chat.contentY
            width: chat.width
            height: chat.height
            visible: messageActionsC.pinned && messageActionsC.positioned
            z: 9
            onClicked: messageActionsC.dismiss()
        }
        Control {
            id: messageActionsC

            property Item attached: null
            // use comma to update on scroll
            property var model: null
            property bool pinned: false
            property bool positioned: false
            property Item anchorItem: null

            function dismiss() {
                pinned = false;
                attached = null;
                anchorItem = null;
                positioned = false;
            }

            function scheduleReposition() {
                if (!visible || !attached || !anchorItem)
                    return;
                if (typeof attached.repositionMessageActions !== "function")
                    return;

                // Hide briefly while coordinates are recalculated, then reveal
                // only after the new position has been committed.
                positioned = false;

                // Reposition in a later frame so we can react to late-arriving
                // intrinsic-size/layout updates that happen after visibility flips.
                Qt.callLater(function () {
                    if (visible && attached && anchorItem)
                        attached.repositionMessageActions(anchorItem, pinned, 0);
                });
            }

            hoverEnabled: true
            padding: Nheko.paddingMedium
            // Keep the control in the layout pass before first placement so
            // implicitWidth/implicitHeight can settle. Opacity gates first paint.
            visible: Settings.timelineMessageActionsActivationPolicy !== Settings.timelineMessageActionsActivationPolicy.Never && !!attached && (pinned || Settings.timelineMessageActionsActivationPolicy === Settings.timelineMessageActionsActivationPolicy.OnHover)
            opacity: positioned ? 1 : 0
            enabled: positioned
            z: 10
            parent: chat.contentItem
            // No anchors — x/y set imperatively by the message styles
            onWidthChanged: scheduleReposition()
            onHeightChanged: scheduleReposition()
            onImplicitWidthChanged: scheduleReposition()
            onImplicitHeightChanged: scheduleReposition()

            background: Rectangle {
                border.color: palette.buttonText
                border.width: 1
                color: palette.window
                radius: padding
            }
            contentItem: MessageActionsToolbar {
                chatRoot: chatRoot
                emojiPopup: emojiPopup
                filteredTimeline: filteredTimeline
                messageActionsControl: messageActionsC
                messageContextMenu: messageContextMenuC
                messageModel: messageActionsC.model
                roomModel: room
                topBar: topBar
            }
        }
        Shortcut {
            sequences: [StandardKey.MoveToPreviousPage]

            onActivated: {
                chat.keepPinnedToBottom = false;
                chat.contentY = chat.contentY - chat.height * 0.9;
                chat.returnToBounds();
            }
        }
        Shortcut {
            sequences: [StandardKey.MoveToNextPage]

            onActivated: {
                chat.keepPinnedToBottom = false;
                chat.contentY = chat.contentY + chat.height * 0.9;
                chat.returnToBounds();
            }
        }
        Shortcut {
            sequences: [StandardKey.Cancel]

            onActivated: {
                if (room.input.uploads.length > 0)
                    room.input.declineUploads();
                else if (room.reply)
                    room.reply = undefined;
                else if (room.edit)
                    room.edit = undefined;
                else
                    room.thread = undefined;
                TimelineManager.focusMessageInput();
            }
        }

        // These shortcuts use the room timeline because switching to threads and out is annoying otherwise.
        // Better solution welcome.
        Shortcut {
            sequence: "Alt+Up"

            onActivated: room.reply = room.indexToId(room.reply ? room.idToIndex(room.reply) + 1 : 0)
        }
        Shortcut {
            sequence: "Alt+Down"

            onActivated: {
                var idx = room.reply ? room.idToIndex(room.reply) - 1 : -1;
                room.reply = idx >= 0 ? room.indexToId(idx) : null;
            }
        }
        Shortcut {
            sequence: "Alt+F"

            onActivated: {
                if (room.reply) {
                    chatRoot.openForwardDialog(room.reply);
                    room.reply = null;
                }
            }
        }
        Shortcut {
            sequence: "Ctrl+E"

            onActivated: {
                room.edit = room.reply;
            }
        }
        Timer {
            id: readTimer

            interval: 1000

            // force current read index to update
            onTriggered: {
                if (room)
                    room.setCurrentIndex(room.currentIndex);
            }
        }
    }
    MessageContextMenu {
        id: messageContextMenuC

        chatRoot: chatRoot
        filteredTimelineModel: filteredTimeline
        roomModel: room
        topBar: topBar
    }
    Component {
        id: forwardCompleterComponent

        ForwardCompleter {
            roomSource: room
            timelineSource: timeline
            timelineViewSource: timelineView
        }
    }
    ReplyContextMenu {
        id: replyContextMenuC

        roomModel: room
    }
    TimelineToEndButton {
        chatList: chat
        scrollbarItem: scrollbar
    }
}
