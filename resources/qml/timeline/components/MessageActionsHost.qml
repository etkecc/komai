// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import cc.etke.komai

Item {
    id: root

    required property var chatList
    required property var chatRoot
    required property var emojiPopup
    required property var filteredTimeline
    required property var roomModel
    required property var topBar
    required property var messageContextMenu
    property alias control: messageActionsC

    // Click-outside overlay: dismisses the action bar when clicking
    // anywhere outside it. Parented to chatList.contentItem (same as
    // the action bar) so z-ordering works: the bar at z:10 renders
    // above the overlay at z:9, allowing button clicks and hovers
    // to reach the bar normally. The overlay tracks the visible
    // viewport via chatList.contentY / chatList.width / chatList.height.
    MouseArea {
        parent: root.chatList.contentItem
        x: 0
        y: root.chatList.contentY
        width: root.chatList.width
        height: root.chatList.height
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
        padding: 0
        leftInset: 0
        rightInset: 0
        topInset: 0
        bottomInset: 0
        // Keep the control in the layout pass before first placement so
        // implicitWidth/implicitHeight can settle. Opacity gates first paint.
        visible: Settings.timelineMessageActionsActivationPolicy !== Settings.TimelineMessageActionsActivationPolicy.Never && !!attached && (pinned || Settings.timelineMessageActionsActivationPolicy === Settings.TimelineMessageActionsActivationPolicy.OnHover)
        opacity: positioned ? 1 : 0
        enabled: positioned
        z: 10
        parent: root.chatList.contentItem
        // No anchors — x/y set imperatively by the message styles
        onWidthChanged: scheduleReposition()
        onHeightChanged: scheduleReposition()
        onImplicitWidthChanged: scheduleReposition()
        onImplicitHeightChanged: scheduleReposition()

        background: TimelineFloatingActionBarBackground {
        }
        contentItem: MessageActionsToolbar {
            id: messageActionsToolbar
            chatRoot: root.chatRoot
            emojiPopup: root.emojiPopup
            filteredTimeline: root.filteredTimeline
            messageActionsControl: messageActionsC
            messageContextMenu: root.messageContextMenu
            messageModel: messageActionsC.model
            roomModel: root.roomModel
            topBar: root.topBar
        }
    }
}
