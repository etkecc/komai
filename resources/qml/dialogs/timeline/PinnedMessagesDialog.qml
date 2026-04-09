// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components"
import "../../timeline/styles/bubble"
import "../../timeline/styles/plain"
import "../../timeline/components" as TimelineComponents
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai 1.0

OverlayDialog {
    id: root

    required property var room
    required property string roomId

    readonly property int pinCount: room ? room.pinnedMessages.length : 0

    title: pinCount > 0 ? qsTr("Pinned messages (%1)").arg(pinCount) : qsTr("Pinned messages")
    titleIcon: ":/icons/icons/ui/pin.svg"
    overlayDialogMaxWidthRatio: 0.8
    width: {
        const vp = overlayDialogViewport;
        const parentWidth = vp ? vp.width : 760;
        const viewportMax = Math.max(240, parentWidth - Komai.paddingLarge * 2);
        const ratioMax = Math.max(240, Math.floor(parentWidth * overlayDialogMaxWidthRatio));
        return Math.min(viewportMax, ratioMax);
    }

    onPinCountChanged: rebuildModel()

    Component.onCompleted: rebuildModel()

    function dayKey(timestamp) {
        return timestamp.getFullYear() * 10000 + (timestamp.getMonth() + 1) * 100 + timestamp.getDate();
    }

    function enrichWithPrevious(eventsInDisplayOrder) {
        const withPrevious = [];
        for (let i = 0; i < eventsInDisplayOrder.length; i++) {
            const current = eventsInDisplayOrder[i];
            const previous = (i + 1 < eventsInDisplayOrder.length) ? eventsInDisplayOrder[i + 1] : null;
            withPrevious.push(Object.assign({}, current, {
                previousDay: previous ? previous.day : current.day,
                previousTimestamp: previous ? previous.timestamp : current.timestamp,
                previousIsStateEvent: previous ? previous.isStateEvent : true,
                previousUserId: previous ? previous.userId : ""
            }));
        }
        return withPrevious;
    }

    function rebuildModel() {
        const model = TimelineManager.matrixTimelineModel;
        if (!model || !root.room) {
            chat.model = [];
            return;
        }

        const pinnedIds = root.room.pinnedMessages;
        const currentUserId = (Komai.currentUser && Komai.currentUser.userid)
            ? String(Komai.currentUser.userid) : "";
        const events = [];

        for (let i = 0; i < pinnedIds.length; i++) {
            const eventId = String(pinnedIds[i] || "");
            if (!eventId) continue;

            const row = model.rowForEventId(eventId);
            if (row < 0) continue;

            const item = model.itemAt(row);
            if (!item) continue;

            const preview = model.previewDataForEvent(eventId);
            if (!preview || !preview.eventId) continue;

            const tsMs = Number(item.timestamp || 0);
            const ts = new Date(tsMs);

            events.push(Object.assign({}, preview, {
                room: null,
                day: dayKey(ts),
                timestamp: ts,
                isSender: currentUserId.length > 0 && String(preview.userId || "") === currentUserId,
                isHiddenEvent: false,
                isThreadRoot: false,
                reactions: [],
                status: MtxEvent.Empty,
                trustlevel: 0,
                notificationlevel: MtxEvent.Empty,
                userPowerlevel: 0,
                avatarUrl: String(item.senderAvatarUrl || preview.avatarUrl || "")
            }));
        }

        // Sort chronologically (oldest first in display = bottom-to-top)
        events.sort(function(a, b) { return a.timestamp.getTime() - b.timestamp.getTime(); });
        chat.model = enrichWithPrevious(events.reverse());
    }

    // Minimal stub for message context menu (dialogs don't need real ones)
    QtObject {
        id: messageContextMenuStub
        function show() {}
        function close() {}
    }

    QtObject {
        id: replyContextMenuStub
        function show() {}
        function close() {}
    }

    // Minimal stub for message actions host
    Control {
        id: messageActionsStub
        property Item attached: null
        property Item anchorItem: null
        property var model: null
        property string activationMode: ""
        property bool pinned: false
        property bool positioned: false
        function dismiss() {
            pinned = false;
            attached = null;
            anchorItem = null;
            positioned = false;
        }
        visible: false
        width: 0
        height: 0
    }

    ListView {
        id: chat

        property int delegateMaxWidth: Math.max(120, width - 2 * Komai.paddingMedium)

        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(contentHeight + topMargin + bottomMargin, 500)
        Layout.maximumHeight: 500
        clip: true
        boundsBehavior: Flickable.StopAtBounds
        interactive: contentHeight > height
        topMargin: Komai.paddingSmall
        bottomMargin: Komai.paddingSmall
        spacing: 2
        verticalLayoutDirection: ListView.BottomToTop

        ScrollBar.vertical: ScrollBar {
            policy: Settings.uiScrollbarPolicy
        }

        Component {
            id: plainMessageStyle

            TimelinePlainMessageStyle {
                required property var modelData
                isHiddenEvent: modelData && modelData.isHiddenEvent !== undefined ? modelData.isHiddenEvent : false
                metadataActionsEnabled: false
                messageActions: messageActionsStub
                messageContextMenu: messageContextMenuStub
                previewData: modelData
                roomModelOverride: root.room
                replyContextMenu: replyContextMenuStub
                scrolledToThis: false
            }
        }

        Component {
            id: bubbleMessageStyle

            TimelineBubbleMessageStyle {
                required property var modelData
                isHiddenEvent: modelData && modelData.isHiddenEvent !== undefined ? modelData.isHiddenEvent : false
                metadataActionsEnabled: false
                messageActions: messageActionsStub
                messageContextMenu: messageContextMenuStub
                previewData: modelData
                roomModelOverride: root.room
                replyContextMenu: replyContextMenuStub
                scrolledToThis: false
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

        delegate: styleDelegateFor(Settings.timelineMessagesStyle, Settings.timelineMessagesLayoutPositioning)
    }

    ColumnLayout {
        Layout.fillWidth: true
        Layout.topMargin: Komai.paddingMedium
        Layout.bottomMargin: Komai.paddingMedium
        spacing: Komai.paddingSmall
        visible: !root.room || root.room.pinnedMessages.length === 0

        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("No pinned messages")
            color: palette.buttonText
            font.pointSize: Settings.uiFontSizePt * 1.2
            font.bold: true
        }

        Label {
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Important messages can be pinned (if you have privileges to do so) and they will show up here.")
            color: palette.placeholderText
            font.pointSize: Settings.uiFontSizePt
            wrapMode: Text.WordWrap
        }
    }
}
