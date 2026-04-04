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
    property var roomModel: null
    property alias control: messageActionsC
    readonly property var chatContentItem: root.chatList ? root.chatList.contentItem : null

    // Click-outside overlay: dismisses the action bar when clicking
    // anywhere outside it. Parented to chatList.contentItem (same as
    // the action bar) so z-ordering works: the bar at z:10 renders
    // above the overlay at z:9, allowing button clicks and hovers
    // to reach the bar normally. The overlay tracks the visible
    // viewport via chatList.contentY / chatList.width / chatList.height.
    MouseArea {
        parent: root.chatContentItem
        x: 0
        y: root.chatList ? root.chatList.contentY : 0
        width: root.chatList ? root.chatList.width : 0
        height: root.chatList ? root.chatList.height : 0
        visible: !!root.chatContentItem && messageActionsC.pinned && messageActionsC.positioned
        z: 9
        onClicked: messageActionsC.dismiss()
    }
    Control {
        id: messageActionsC

        property Item attached: null
        // use comma to update on scroll
        property var model: null
        property var roomModelOverride: null
        property string activationMode: ""
        readonly property bool pinned: activationMode === "button" || activationMode === "keyboard"
        readonly property bool keyboardActive: activationMode === "keyboard"
        property bool positioned: false
        property Item anchorItem: null

        function dismiss() {
            activationMode = "";
            attached = null;
            anchorItem = null;
            roomModelOverride = null;
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
                    attached.repositionMessageActions(anchorItem, activationMode, 0);
            });
        }

        function focusFirstVisibleButton() {
            return messageActionsToolbar.focusFirstVisibleButton();
        }

        function focusLastVisibleButton() {
            return messageActionsToolbar.focusLastVisibleButton();
        }

        function moveFocus(step) {
            return messageActionsToolbar.moveFocus(step);
        }

        function usesTwoRowLayout() {
            return messageActionsToolbar.twoRowMode;
        }

        function activateFocusedButton() {
            return messageActionsToolbar.activateFocusedButton();
        }

        hoverEnabled: true
        leftPadding: 0
        rightPadding: 0
        topPadding: 0
        bottomPadding: 0
        leftInset: 0
        rightInset: 0
        topInset: 0
        bottomInset: 0
        implicitWidth: contentItem ? contentItem.implicitWidth : 0
        implicitHeight: contentItem ? contentItem.implicitHeight : 0
        // Keep the control in the layout pass before first placement so
        // implicitWidth/implicitHeight can settle. Opacity gates first paint.
        visible: Settings.timelineMessageActionsActivationPolicy !== Settings.TimelineMessageActionsActivationPolicy.Never
            && !!attached
            && (pinned
                || (activationMode === "hover"
                    && Settings.timelineMessageActionsActivationPolicy === Settings.TimelineMessageActionsActivationPolicy.OnHover))
        opacity: positioned ? 1 : 0
        enabled: positioned
        z: 10
        parent: root.chatContentItem
        // No anchors — x/y set imperatively by the message styles
        onPositionedChanged: {
            if (positioned && keyboardActive)
                Qt.callLater(focusFirstVisibleButton);
        }
        onWidthChanged: scheduleReposition()
        onHeightChanged: scheduleReposition()
        onImplicitWidthChanged: scheduleReposition()
        onImplicitHeightChanged: scheduleReposition()

        background: TimelineFloatingActionBarBackground {
            barColor: palette.alternateBase
            barRadius: Komai.paddingSmall
            barBorderColor: Komai.theme.separator
            barBorderWidth: 1
        }
        contentItem: Item {
            implicitWidth: messageActionsToolbar.implicitWidth
            implicitHeight: messageActionsToolbar.implicitHeight

            MessageActionsToolbar {
                id: messageActionsToolbar

                anchors.centerIn: parent
                chatRoot: root.chatRoot
                emojiPopup: root.emojiPopup
                filteredTimeline: root.filteredTimeline
                messageActionsControl: messageActionsC
                messageModel: messageActionsC.model
                roomModel: messageActionsC.roomModelOverride
                    ? messageActionsC.roomModelOverride
                    : (messageActionsC.model && messageActionsC.model.roomModelOverride)
                        ? messageActionsC.model.roomModelOverride
                        : root.roomModel
            }
        }
    }
}
