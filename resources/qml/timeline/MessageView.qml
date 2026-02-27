// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "./styles/bubble"
import "./styles/plain"
import "../components"
import "../ui"
import "../dialogs"
import "../dialogs/moderation"
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
            property alias model: row.model
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
            contentItem: RowLayout {
                id: row

                property var model
                property int itemPadding: Math.round(messageActionsC.padding / 2)

                spacing: 0

                // --- Pinned reactions (from user setting, comma-separated, max 10) ---
                Repeater {
                    model: Settings.timelineMessageActionsPinnedReactions.split(",").map(function(s) { return s.trim(); }).filter(function(s) { return s.length > 0; }).slice(0, 10)
                    visible: room ? room.permissions.canSend(MtxEvent.Reaction) : false

                    delegate: AbstractButton {
                        id: btnPinned

                        property color buttonTextColor: palette.buttonText
                        property color highlightColor: palette.highlight
                        required property string modelData
                        property bool showImage: modelData.startsWith("mxc://")

                        Layout.alignment: Qt.AlignBottom
                        focusPolicy: Qt.NoFocus
                        leftPadding: row.itemPadding
                        rightPadding: row.itemPadding
                        height: showImage ? 32 : btnTextPinned.implicitHeight
                        implicitHeight: showImage ? 32 : btnTextPinned.implicitHeight
                        implicitWidth: (showImage ? 32 : btnTextPinned.implicitWidth) + 2 * row.itemPadding
                        width: (showImage ? 32 : btnTextPinned.implicitWidth) + 2 * row.itemPadding

                        onClicked: {
                            room.input.reaction(row.model.eventId, modelData);
                            TimelineManager.focusMessageInput();
                            messageActionsC.dismiss();
                        }

                        Label {
                            id: btnTextPinned

                            anchors.centerIn: parent
                            color: btnPinned.hovered ? btnPinned.highlightColor : btnPinned.buttonTextColor
                            font.pixelSize: 32
                            font.family: Settings.uiFontEmojiFamily
                            horizontalAlignment: Text.AlignHCenter
                            padding: 0
                            text: TimelineManager.htmlEscape(btnPinned.modelData)
                            verticalAlignment: Text.AlignVCenter
                            visible: !btnPinned.showImage
                        }
                        Image {
                            anchors.fill: parent
                            fillMode: Image.PreserveAspectFit
                            source: btnPinned.showImage ? (btnPinned.modelData.replace("mxc://", "image://MxcImage/") + "?scale") : ""
                            sourceSize.height: btnPinned.height
                            sourceSize.width: btnPinned.width
                        }
                        NhekoCursorShape {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                        }
                        Ripple {
                            color: Qt.rgba(btnPinned.buttonTextColor.r, btnPinned.buttonTextColor.g, btnPinned.buttonTextColor.b, 0.5)
                        }
                        HoverPulseAnimation {
                            id: pinnedPulseAnim

                            targetItem: btnPinned
                        }
                        onHoveredChanged: {
                            if (hovered)
                                pinnedPulseAnim.pulse();
                        }
                    }
                }
                // --- Recent reactions (from user history, excluding pinned; total pinned+recent capped at 10) ---
                Repeater {
                    property var pinnedSet: Settings.timelineMessageActionsPinnedReactions.split(",").map(function(s) { return s.trim(); }).filter(function(s) { return s.length > 0; }).slice(0, 10)
                    model: Settings.recentReactions.filter(function(r) { return pinnedSet.indexOf(r) < 0; }).slice(0, Math.max(0, 10 - pinnedSet.length))
                    visible: room ? room.permissions.canSend(MtxEvent.Reaction) : false

                    delegate: AbstractButton {
                        id: btnRecent

                        property color buttonTextColor: palette.buttonText
                        property color highlightColor: palette.highlight
                        required property string modelData
                        property bool showImage: modelData.startsWith("mxc://")

                        Layout.alignment: Qt.AlignBottom
                        focusPolicy: Qt.NoFocus
                        leftPadding: row.itemPadding
                        rightPadding: row.itemPadding
                        height: showImage ? 32 : btnTextRecent.implicitHeight
                        implicitHeight: showImage ? 32 : btnTextRecent.implicitHeight
                        implicitWidth: (showImage ? 32 : btnTextRecent.implicitWidth) + 2 * row.itemPadding
                        width: (showImage ? 32 : btnTextRecent.implicitWidth) + 2 * row.itemPadding

                        onClicked: {
                            room.input.reaction(row.model.eventId, modelData);
                            TimelineManager.focusMessageInput();
                            messageActionsC.dismiss();
                        }

                        Label {
                            id: btnTextRecent

                            anchors.centerIn: parent
                            color: btnRecent.hovered ? btnRecent.highlightColor : btnRecent.buttonTextColor
                            font.pixelSize: 32
                            font.family: Settings.uiFontEmojiFamily
                            horizontalAlignment: Text.AlignHCenter
                            padding: 0
                            text: TimelineManager.htmlEscape(btnRecent.modelData)
                            verticalAlignment: Text.AlignVCenter
                            visible: !btnRecent.showImage
                        }
                        Image {
                            anchors.fill: parent
                            fillMode: Image.PreserveAspectFit
                            source: btnRecent.showImage ? (btnRecent.modelData.replace("mxc://", "image://MxcImage/") + "?scale") : ""
                            sourceSize.height: btnRecent.height
                            sourceSize.width: btnRecent.width
                        }
                        NhekoCursorShape {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                        }
                        Ripple {
                            color: Qt.rgba(btnRecent.buttonTextColor.r, btnRecent.buttonTextColor.g, btnRecent.buttonTextColor.b, 0.5)
                        }
                        HoverPulseAnimation {
                            id: recentPulseAnim

                            targetItem: btnRecent
                        }
                        onHoveredChanged: {
                            if (hovered)
                                recentPulseAnim.pulse();
                        }
                    }
                }
                ImageButton {
                    id: reactButton

                    ToolTip.delay: Nheko.tooltipDelay
                    ToolTip.text: qsTr("React")
                    ToolTip.visible: hovered
                    hoverEnabled: true
                    hoverPulse: true
                    image: ":/icons/icons/ui/smile-add.svg"
                    visible: room ? room.permissions.canSend(MtxEvent.Reaction) : false
                    leftPadding: row.itemPadding
                    rightPadding: row.itemPadding
                    Layout.preferredWidth: 32 + 2 * row.itemPadding
                    Layout.preferredHeight: 32

                    onClicked: emojiPopup.visible ? emojiPopup.close() : emojiPopup.show(reactButton, room.roomId, function (plaintext, markdown) {
                            var event_id = row.model ? row.model.eventId : "";
                            room.input.reaction(event_id, plaintext);
                            TimelineManager.focusMessageInput();
                        })
                }
                ImageButton {
                    ToolTip.delay: Nheko.tooltipDelay
                    ToolTip.text: qsTr("Edit")
                    ToolTip.visible: hovered
                    buttonTextColor: palette.buttonText
                    hoverEnabled: true
                    hoverPulse: true
                    image: ":/icons/icons/ui/edit.svg"
                    visible: !!row.model && row.model.isEditable
                    leftPadding: row.itemPadding
                    rightPadding: row.itemPadding
                    Layout.preferredWidth: 32 + 2 * row.itemPadding
                    Layout.preferredHeight: 32

                    onClicked: {
                        if (row.model.isEditable)
                            room.edit = row.model.eventId;
                        messageActionsC.dismiss();
                    }
                }
                ImageButton {
                    ToolTip.delay: Nheko.tooltipDelay
                    ToolTip.text: (row.model && row.model.threadId) ? qsTr("Reply in thread") : qsTr("New thread")
                    ToolTip.visible: hovered
                    hoverEnabled: true
                    hoverPulse: true
                    image: ":/icons/icons/ui/thread.svg"
                    visible: room ? room.permissions.canSend(MtxEvent.TextMessage) : false
                    leftPadding: row.itemPadding
                    rightPadding: row.itemPadding
                    Layout.preferredWidth: 32 + 2 * row.itemPadding
                    Layout.preferredHeight: 32

                    onClicked: {
                        room.thread = (row.model.threadId || row.model.eventId);
                        messageActionsC.dismiss();
                    }
                }
                ImageButton {
                    ToolTip.delay: Nheko.tooltipDelay
                    ToolTip.text: qsTr("Reply")
                    ToolTip.visible: hovered
                    hoverEnabled: true
                    hoverPulse: true
                    image: ":/icons/icons/ui/reply.svg"
                    visible: room ? room.permissions.canSend(MtxEvent.TextMessage) : false
                    leftPadding: row.itemPadding
                    rightPadding: row.itemPadding
                    Layout.preferredWidth: 32 + 2 * row.itemPadding
                    Layout.preferredHeight: 32

                    onClicked: {
                        room.reply = row.model.eventId;
                        messageActionsC.dismiss();
                    }
                }
                ImageButton {
                    ToolTip.delay: Nheko.tooltipDelay
                    ToolTip.text: qsTr("Forward")
                    ToolTip.visible: hovered
                    hoverEnabled: true
                    hoverPulse: true
                    image: ":/icons/icons/ui/reply.svg"
                    visible: !!row.model && (row.model.type == MtxEvent.ImageMessage || row.model.type == MtxEvent.VideoMessage || row.model.type == MtxEvent.AudioMessage || row.model.type == MtxEvent.FileMessage || row.model.type == MtxEvent.Sticker || row.model.type == MtxEvent.TextMessage || row.model.type == MtxEvent.LocationMessage || row.model.type == MtxEvent.EmoteMessage || row.model.type == MtxEvent.NoticeMessage)
                    leftPadding: row.itemPadding
                    rightPadding: row.itemPadding
                    Layout.preferredWidth: 32 + 2 * row.itemPadding
                    Layout.preferredHeight: 32
                    transform: Scale { origin.x: 16 + row.itemPadding; xScale: -1 }

                    onClicked: {
                        var forwardMess = forwardCompleterComponent.createObject(timelineRoot);
                        forwardMess.setMessageEventId(row.model.eventId);
                        forwardMess.open();
                        timelineRoot.destroyOnClose(forwardMess);
                        messageActionsC.dismiss();
                    }
                }
                ImageButton {
                    ToolTip.delay: Nheko.tooltipDelay
                    ToolTip.text: qsTr("Go to message")
                    ToolTip.visible: hovered
                    buttonTextColor: palette.buttonText
                    hoverEnabled: true
                    hoverPulse: true
                    image: ":/icons/icons/ui/go-to.svg"
                    visible: !!row.model && filteredTimeline.filterByContent
                    leftPadding: row.itemPadding
                    rightPadding: row.itemPadding
                    Layout.preferredWidth: 32 + 2 * row.itemPadding
                    Layout.preferredHeight: 32

                    onClicked: {
                        topBar.searchString = "";
                        room.showEvent(row.model.eventId);
                        messageActionsC.dismiss();
                    }
                }
                ImageButton {
                    id: optionsButton

                    ToolTip.delay: Nheko.tooltipDelay
                    ToolTip.text: qsTr("Options")
                    ToolTip.visible: hovered
                    hoverEnabled: true
                    hoverPulse: true
                    image: ":/icons/icons/ui/options-circle.svg"
                    leftPadding: row.itemPadding
                    rightPadding: row.itemPadding
                    Layout.preferredWidth: 32 + 2 * row.itemPadding
                    Layout.preferredHeight: 32

                    onClicked: messageContextMenuC.show(row.model.eventId, row.model.threadId, row.model.type, row.model.isSender, row.model.isEncrypted, row.model.isEditable, "", row.model.body, optionsButton)
                }
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
                    var forwardMess = forwardCompleterComponent.createObject(timelineRoot);
                    forwardMess.setMessageEventId(room.reply);
                    forwardMess.open();
                    room.reply = null;
                    timelineRoot.destroyOnClose(forwardMess);
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
    Menu {
        id: messageContextMenuC

        property string eventId
        property int eventType
        property bool isEditable
        property bool isEncrypted
        property bool isSender
        property string link
        property string text
        property string threadId

        function show(eventId_, threadId_, eventType_, isSender_, isEncrypted_, isEditable_, link_, text_, showAt_) {
            eventId = eventId_;
            threadId = threadId_;
            eventType = eventType_;
            isEncrypted = isEncrypted_;
            isEditable = isEditable_;
            isSender = isSender_;
            if (text_)
                text = text_;
            else
                text = "";
            if (link_)
                link = link_;
            else
                link = "";

            messageActionsCFilter.updateTarget();

            if (showAt_)
                popup(showAt_);
            else
                popup();
        }

        Component {
            id: removeReason

            InputDialog {
                id: removeReasonDialog

                property string eventId

                prompt: qsTr("Enter reason for removal or hit enter for no reason:")
                title: qsTr("Reason for removal")

                onAccepted: function (text) {
                    room.redactEvent(eventId, text);
                }
            }
        }
        Component {
            id: reportDialog

            ReportMessage {}
        }

        Component.onCompleted: {
            if (messageContextMenuC.popupType != undefined) {
                messageContextMenuC.popupType = 2; // Popup.Native with fallback on older Qt (<6.8.0)
            }
        }

        NhekoMenuVisibilityFilter on contentData {
            id: messageActionsCFilter

            Component {
                MenuItem {
                    text: qsTr("Go to &message")
                    visible: filteredTimeline.filterByContent

                    onTriggered: function () {
                        topBar.searchString = "";
                        room.showEvent(messageContextMenuC.eventId);
                    }
                }
            }
            Component {
                MenuItem {
                    text: qsTr("&Copy")
                    visible: messageContextMenuC.text

                    onTriggered: Clipboard.text = messageContextMenuC.text
                }
            }
            Component {
                MenuItem {
                    text: qsTr("Copy &link location")
                    visible: messageContextMenuC.link

                    onTriggered: Clipboard.text = messageContextMenuC.link
                }
            }
            Component {
                MenuItem {
                    id: reactionOption

                    text: qsTr("Re&act")
                    visible: room ? room.permissions.canSend(MtxEvent.Reaction) : false

                    onTriggered: emojiPopup.visible ? emojiPopup.close() : emojiPopup.show(null, room.roomId, function (plaintext, markdown) {
                        room.input.reaction(messageContextMenuC.eventId, plaintext);
                        TimelineManager.focusMessageInput();
                    })
                }
            }
            Component {
                MenuItem {
                    text: qsTr("Repl&y")
                    visible: room ? room.permissions.canSend(MtxEvent.TextMessage) : false

                    onTriggered: room.reply = (messageContextMenuC.eventId)
                }
            }
            Component {
                MenuItem {
                    text: qsTr("&Edit")
                    visible: messageContextMenuC.isEditable && (room ? room.permissions.canSend(MtxEvent.TextMessage) : false)

                    onTriggered: room.edit = (messageContextMenuC.eventId)
                }
            }
            Component {
                MenuItem {
                    text: qsTr("&Thread")
                    visible: (room ? room.permissions.canSend(MtxEvent.TextMessage) : false)

                    onTriggered: room.thread = (messageContextMenuC.threadId || messageContextMenuC.eventId)
                }
            }
            Component {
                MenuItem {
                    text: visible && room.pinnedMessages.includes(messageContextMenuC.eventId) ? qsTr("Un&pin") : qsTr("&Pin")
                    visible: (room ? room.permissions.canChange(MtxEvent.PinnedEvents) : false)

                    onTriggered: visible && room.pinnedMessages.includes(messageContextMenuC.eventId) ? room.unpin(messageContextMenuC.eventId) : room.pin(messageContextMenuC.eventId)
                }
            }
            Component {
                MenuItem {
                    text: qsTr("&Read receipts")

                    onTriggered: room.showReadReceipts(messageContextMenuC.eventId)
                }
            }
            Component {
                MenuItem {
                    text: qsTr("&Forward")
                    visible: messageContextMenuC.eventType == MtxEvent.ImageMessage || messageContextMenuC.eventType == MtxEvent.VideoMessage || messageContextMenuC.eventType == MtxEvent.AudioMessage || messageContextMenuC.eventType == MtxEvent.FileMessage || messageContextMenuC.eventType == MtxEvent.Sticker || messageContextMenuC.eventType == MtxEvent.TextMessage || messageContextMenuC.eventType == MtxEvent.LocationMessage || messageContextMenuC.eventType == MtxEvent.EmoteMessage || messageContextMenuC.eventType == MtxEvent.NoticeMessage

                    onTriggered: {
                        var forwardMess = forwardCompleterComponent.createObject(timelineRoot);
                        forwardMess.setMessageEventId(messageContextMenuC.eventId);
                        forwardMess.open();
                        timelineRoot.destroyOnClose(forwardMess);
                    }
                }
            }
            Component {
                MenuItem {
                    text: qsTr("&Mark as read")

                    onTriggered: room.markEventAsRead(messageContextMenuC.eventId)
                }
            }
            Component {
                MenuItem {
                    text: qsTr("View raw message")

                    onTriggered: room.viewRawMessage(messageContextMenuC.eventId)
                }
            }
            Component {
                MenuItem {
                    text: qsTr("View decrypted raw message")
                    // TODO(Nico): Fix this still being iterated over, when using keyboard to select options
                    visible: messageContextMenuC.isEncrypted

                    onTriggered: room.viewDecryptedRawMessage(messageContextMenuC.eventId)
                }
            }
            Component {
                MenuItem {
                    text: qsTr("Remo&ve message")
                    visible: (room ? room.permissions.canRedact() : false) || messageContextMenuC.isSender

                    onTriggered: function () {
                        var dialog = removeReason.createObject(timelineRoot);
                        dialog.eventId = messageContextMenuC.eventId;
                        dialog.show();
                        dialog.forceActiveFocus();
                        timelineRoot.destroyOnClose(dialog);
                    }
                }
            }
            Component {
                MenuItem {
                    text: qsTr("Report message")
                    onTriggered: function () {
                        var dialog = reportDialog.createObject(timelineRoot, {"eventId": messageContextMenuC.eventId});
                        dialog.show();
                        dialog.forceActiveFocus();
                        timelineRoot.destroyOnClose(dialog);
                    }
                }
            }
            Component {
                MenuItem {
                    text: qsTr("&Save as")
                    visible: messageContextMenuC.eventType == MtxEvent.ImageMessage || messageContextMenuC.eventType == MtxEvent.VideoMessage || messageContextMenuC.eventType == MtxEvent.AudioMessage || messageContextMenuC.eventType == MtxEvent.FileMessage || messageContextMenuC.eventType == MtxEvent.Sticker

                    onTriggered: room.saveMedia(messageContextMenuC.eventId)
                }
            }
            Component {
                MenuItem {
                    text: qsTr("&Open in external program")
                    visible: messageContextMenuC.eventType == MtxEvent.ImageMessage || messageContextMenuC.eventType == MtxEvent.VideoMessage || messageContextMenuC.eventType == MtxEvent.AudioMessage || messageContextMenuC.eventType == MtxEvent.FileMessage || messageContextMenuC.eventType == MtxEvent.Sticker

                    onTriggered: room.openMedia(messageContextMenuC.eventId)
                }
            }
            Component {
                MenuItem {
                    text: qsTr("Copy link to eve&nt")
                    visible: messageContextMenuC.eventId

                    onTriggered: room.copyLinkToEvent(messageContextMenuC.eventId)
                }
            }
        }
    }
    Component {
        id: forwardCompleterComponent

        ForwardCompleter {
            roomSource: room
            timelineSource: timeline
            timelineViewSource: timelineView
        }
    }
    Menu {
        id: replyContextMenuC

        property string eventId
        property string link
        property string text

        function show(text_, link_, eventId_) {
            text = text_;
            link = link_;
            eventId = eventId_;

            replyContextMenuCFilter.updateTarget();
            popup();
        }

        Component.onCompleted: {
            if (replyContextMenuC.popupType != undefined) {
                replyContextMenuC.popupType = 2; // Popup.Native with fallback on older Qt (<6.8.0)
            }
        }


        NhekoMenuVisibilityFilter on contentData {
            id: replyContextMenuCFilter

            Component {
                MenuItem {
                    text: qsTr("&Copy")
                    visible: replyContextMenuC.text

                    onTriggered: Clipboard.text = replyContextMenuC.text
                }
            }
            Component {
                MenuItem {
                    text: qsTr("Copy &link location")
                    visible: replyContextMenuC.link

                    onTriggered: Clipboard.text = replyContextMenuC.link
                }
            }
            Component {
                MenuItem {
                    text: qsTr("&Go to quoted message")
                    visible: true

                    onTriggered: room.showEvent(replyContextMenuC.eventId)
                }
            }
        }
    }
    RoundButton {
        id: toEndButton

        property int fullWidth: 40

        flat: true
        height: width
        hoverEnabled: true
        radius: width / 2
        width: 0

        background: Rectangle {
            border.color: toEndButton.hovered ? palette.highlight : palette.buttonText
            border.width: 1
            color: toEndButton.down ? palette.highlight : palette.button
            opacity: enabled ? 1 : 0.3
            radius: toEndButton.radius
        }
        states: [
            State {
                name: ""

                PropertyChanges {
                    toEndButton.width: 0
                }
            },
            State {
                name: "shown"
                when: !chat.atYEnd

                PropertyChanges {
                    toEndButton.width: toEndButton.fullWidth
                }
            }
        ]
        transitions: Transition {
            from: ""
            reversible: true
            to: "shown"

            SequentialAnimation {
                PauseAnimation {
                    duration: 500
                }
                PropertyAnimation {
                    duration: 200
                    easing.type: Easing.InOutQuad
                    properties: "width"
                    target: toEndButton
                }
            }
        }

        onClicked: function () {
            chat.keepPinnedToBottom = true;
            chat.positionViewAtBeginning();
            TimelineManager.focusMessageInput();
            chat.updateLastScroll();
        }

        anchors {
            bottom: parent.bottom
            bottomMargin: Nheko.paddingMedium + (fullWidth - width) / 2
            right: scrollbar.left
            rightMargin: Nheko.paddingMedium + (fullWidth - width) / 2
        }
        Image {
            anchors.fill: parent
            anchors.margins: Nheko.paddingMedium
            fillMode: Image.PreserveAspectFit
            source: "image://colorimage/:/icons/icons/ui/download.svg?" + (toEndButton.down ? palette.highlightedText : palette.buttonText)
        }
    }
}
