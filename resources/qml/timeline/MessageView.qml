// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "./styles/bubble"
import "./styles/plain"
import "./components"
import "../ui"
import QtQuick 2.15
import QtQuick.Controls 2.15
import im.nheko 1.0

Item {
    id: chatRoot

    required property var emojiPopup
    required property var dialogHost
    required property var componentCatalog
    property int availableWidth: width
    property int padding: Nheko.paddingMedium
    property string searchString: ""
    property bool filterByNotifications: false
    readonly property bool filteringInProgress: filteredTimeline.filteringInProgress
    property Room roommodel: room

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

    function showDialogFromComponent(componentRef, properties) {
        var dialogParent = dialogHost || chatRoot;
        var dialog = componentRef.createObject(dialogParent, properties || {});
        if (!dialog)
            return null;
        dialog.show();
        dialog.forceActiveFocus();
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
            roomModel: room
            filteringInProgress: chatRoot.filteringInProgress
            searchString: chatRoot.searchString
        }

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
        MessageActionsHost {
            id: messageActionsHost
            chatList: chat
            chatRoot: chatRoot
            emojiPopup: chatRoot.emojiPopup
            filteredTimeline: filteredTimeline
            roomModel: room
            topBar: topBar
            messageContextMenu: messageContextMenuC
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
        topBar: topBar
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
