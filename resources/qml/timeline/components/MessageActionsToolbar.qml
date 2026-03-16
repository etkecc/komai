// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import cc.etke.komai

Item {
    id: toolbar

    required property var chatRoot
    required property var emojiPopup
    required property var filteredTimeline
    required property var messageActionsControl
    required property var messageModel
    required property var roomModel
    required property var topBar
    property int itemHorizontalPadding: Komai.uiLayoutCompactMode ? Komai.paddingSmall : Komai.paddingMedium
    property int itemVerticalPadding: 0
    property int actionButtonHeight: Komai.listIconSize
    property int actionButtonIconSize: Math.max(14, actionButtonHeight - 2 * itemHorizontalPadding)
    property int labelBreakpointWidth: 600
    readonly property real actionHostWidth: (messageActionsControl && messageActionsControl.parent)
        ? messageActionsControl.parent.width
        : (chatRoot ? chatRoot.width : width)
    readonly property bool isStateEvent: !!messageModel && messageModel.isStateEvent
    readonly property bool canReact: !isStateEvent && (roomModel ? roomModel.permissions.canSend(MtxEvent.Reaction) : false)
    readonly property bool canSendText: !isStateEvent && (roomModel ? roomModel.permissions.canSend(MtxEvent.TextMessage) : false)
    readonly property bool canEdit: !!messageModel && messageModel.isEditable
    readonly property bool canForward: !!messageModel && isForwardableType(messageModel.type)
    readonly property bool canGoToMessage: !!messageModel && filteredTimeline.filterByContent
    readonly property real requiredLabeledWidth: reactionButtonsWidth
        + (canReact ? iconOnlyButtonWidth() : 0)
        + (canReact ? separatorSlotWidth : 0)
        + (canEdit ? labeledButtonWidth(editLabelMetrics.advanceWidth) : 0)
        + (canSendText ? labeledButtonWidth(threadLabelMetrics.advanceWidth) : 0)
        + (canSendText ? labeledButtonWidth(replyLabelMetrics.advanceWidth) : 0)
        + (canForward ? labeledButtonWidth(forwardLabelMetrics.advanceWidth) : 0)
        + (canGoToMessage ? labeledButtonWidth(goToLabelMetrics.advanceWidth) : 0)
        + iconOnlyButtonWidth()
    // Minimum width needed to fit everything on a single row (icon-only, no labels).
    readonly property real requiredSingleRowWidth: reactionButtonsWidth
        + (canReact ? iconOnlyButtonWidth() : 0)
        + (canReact ? separatorSlotWidth : 0)
        + (canEdit ? iconOnlyButtonWidth() : 0)
        + (canSendText ? iconOnlyButtonWidth() : 0)
        + (canSendText ? iconOnlyButtonWidth() : 0)
        + (canForward ? iconOnlyButtonWidth() : 0)
        + (canGoToMessage ? iconOnlyButtonWidth() : 0)
        + iconOnlyButtonWidth()
    readonly property bool twoRowMode: actionHostWidth > 0 && actionHostWidth < requiredSingleRowWidth
    readonly property bool showActionLabels: !twoRowMode
        && actionHostWidth >= labelBreakpointWidth
        && actionHostWidth >= requiredLabeledWidth
    property color actionButtonColor: palette.buttonText
    property color actionButtonActiveColor: palette.brightText
    property color actionButtonHoverBackgroundColor: palette.dark
    readonly property int separatorSlotWidth: Komai.paddingMedium * 2 + 1
    readonly property real reactionButtonsWidth: repeaterItemsWidth(pinnedReactionsRepeater) + repeaterItemsWidth(recentReactionsRepeater)
    readonly property int twoRowSpacing: Komai.paddingSmall

    implicitWidth: twoRowMode
        ? Math.max(actionsRow.implicitWidth, reactionsRow.implicitWidth)
        : (actionsRow.implicitWidth
            + (canReact ? separatorItem.width + reactionsRow.implicitWidth : 0))
    implicitHeight: twoRowMode
        ? actionsRow.implicitHeight + twoRowSpacing + reactionsRow.implicitHeight
        : Math.max(actionsRow.implicitHeight, reactionsRow.implicitHeight)

    function isForwardableType(eventType) {
        return eventType == MtxEvent.ImageMessage || eventType == MtxEvent.VideoMessage || eventType == MtxEvent.AudioMessage || eventType == MtxEvent.FileMessage || eventType == MtxEvent.Sticker || eventType == MtxEvent.TextMessage || eventType == MtxEvent.LocationMessage || eventType == MtxEvent.EmoteMessage || eventType == MtxEvent.NoticeMessage;
    }

    function threadActionLabel() {
        return (toolbar.messageModel && toolbar.messageModel.threadId) ? qsTr("Reply in thread") : qsTr("New thread");
    }

    function repeaterItemsWidth(repeater) {
        if (!repeater || repeater.count <= 0)
            return 0;

        let total = 0;
        for (let i = 0; i < repeater.count; i++) {
            const item = repeater.itemAt(i);
            if (!item || item.visible === false)
                continue;
            total += Math.max(item.implicitWidth || 0, item.width || 0);
        }
        return total;
    }

    function iconOnlyButtonWidth() {
        return actionButtonIconSize + itemHorizontalPadding * 2;
    }

    function labeledButtonWidth(labelAdvanceWidth) {
        return iconOnlyButtonWidth() + Komai.paddingSmall + labelAdvanceWidth;
    }

    TextMetrics {
        id: editLabelMetrics
        font: Qt.font({
            "bold": true
        })
        text: qsTr("Edit")
    }
    TextMetrics {
        id: threadLabelMetrics
        font: Qt.font({
            "bold": true
        })
        text: toolbar.threadActionLabel()
    }
    TextMetrics {
        id: replyLabelMetrics
        font: Qt.font({
            "bold": true
        })
        text: qsTr("Reply")
    }
    TextMetrics {
        id: forwardLabelMetrics
        font: Qt.font({
            "bold": true
        })
        text: qsTr("Forward")
    }
    TextMetrics {
        id: goToLabelMetrics
        font: Qt.font({
            "bold": true
        })
        text: qsTr("Go to message")
    }

    // Reactions row: emoji quick-reactions + React picker button.
    // In two-row mode this is the bottom row (closer to the input area).
    RowLayout {
        id: reactionsRow
        spacing: 0
        visible: toolbar.canReact

        x: toolbar.twoRowMode ? Math.round((toolbar.implicitWidth - reactionsRow.implicitWidth) / 2) : 0
        y: 0

        Repeater {
            id: pinnedReactionsRepeater

            model: toolbar.canReact
                ? Settings.timelineMessageActionsPinnedReactions.split(",").map(function (s) {
                    return s.trim();
                }).filter(function (s) {
                    return s.length > 0;
                }).slice(0, 8)
                : []

            delegate: MessageActionsReactionButton {
                required property var modelData

                reaction: modelData
                messageModel: toolbar.messageModel
                roomModel: toolbar.roomModel
                messageActionsControl: toolbar.messageActionsControl
                actionButtonColor: toolbar.actionButtonColor
                actionButtonActiveColor: toolbar.actionButtonActiveColor
                actionButtonHoverBackgroundColor: toolbar.actionButtonHoverBackgroundColor
                actionButtonIconSize: toolbar.actionButtonIconSize
                actionButtonHeight: toolbar.actionButtonHeight
                itemHorizontalPadding: toolbar.itemHorizontalPadding
                itemVerticalPadding: toolbar.itemVerticalPadding
            }
        }

        Repeater {
            id: recentReactionsRepeater

            property var pinnedSet: toolbar.canReact
                ? Settings.timelineMessageActionsPinnedReactions.split(",").map(function (s) {
                    return s.trim();
                }).filter(function (s) {
                    return s.length > 0;
                }).slice(0, 8)
                : []
            model: toolbar.canReact && toolbar.roomModel
                ? toolbar.roomModel.frequentReactions.filter(function (reaction) {
                    return pinnedSet.indexOf(reaction) < 0;
                }).slice(0, Math.max(0, 8 - pinnedSet.length))
                : []

            delegate: MessageActionsReactionButton {
                required property var modelData

                reaction: modelData
                messageModel: toolbar.messageModel
                roomModel: toolbar.roomModel
                messageActionsControl: toolbar.messageActionsControl
                actionButtonColor: toolbar.actionButtonColor
                actionButtonActiveColor: toolbar.actionButtonActiveColor
                actionButtonHoverBackgroundColor: toolbar.actionButtonHoverBackgroundColor
                actionButtonIconSize: toolbar.actionButtonIconSize
                actionButtonHeight: toolbar.actionButtonHeight
                itemHorizontalPadding: toolbar.itemHorizontalPadding
                itemVerticalPadding: toolbar.itemVerticalPadding
            }
        }

        MessageActionsToolbarButton {
            id: reactButton

            toolbarRef: toolbar
            image: ":/icons/icons/ui/smile-add.svg"
            labelText: ""
            toolTipText: qsTr("React")

            onClicked: emojiPopup.visible ? emojiPopup.close() : emojiPopup.show(reactButton, roomModel.roomId, function (plaintext, markdown) {
                    var eventId = toolbar.messageModel ? toolbar.messageModel.eventId : "";
                    roomModel.input.reaction(eventId, plaintext);
                    TimelineManager.focusMessageInput();
                })
        }
    }

    // Separator between reactions and actions (single-row mode only).
    Item {
        id: separatorItem

        visible: !toolbar.twoRowMode && toolbar.canReact
        width: toolbar.separatorSlotWidth
        height: toolbar.actionButtonHeight
        x: reactionsRow.implicitWidth
        y: 0

        Rectangle {
            id: separator

            anchors.verticalCenter: parent.verticalCenter
            anchors.horizontalCenter: parent.horizontalCenter
            width: 1
            height: Math.max(1, parent.height - Komai.paddingMedium)
            color: Komai.theme.separator
        }
    }

    // Actions row: Edit, Thread, Reply, Forward, Go to, Options.
    // In two-row mode this is the top row.
    RowLayout {
        id: actionsRow
        spacing: 0

        x: toolbar.twoRowMode ? Math.round((toolbar.implicitWidth - actionsRow.implicitWidth) / 2) : (toolbar.canReact ? (reactionsRow.implicitWidth + separatorItem.width) : 0)
        y: toolbar.twoRowMode ? (reactionsRow.height + toolbar.twoRowSpacing) : 0

        MessageActionsToolbarButton {
            toolbarRef: toolbar
            image: ":/icons/icons/ui/edit.svg"
            labelText: toolbar.showActionLabels ? qsTr("Edit") : ""
            toolTipText: qsTr("Edit")
            visible: toolbar.canEdit

            onClicked: {
                if (toolbar.messageModel.isEditable)
                    roomModel.edit = toolbar.messageModel.eventId;
                toolbar.messageActionsControl.dismiss();
            }
        }

        MessageActionsToolbarButton {
            toolbarRef: toolbar
            image: ":/icons/icons/ui/thread.svg"
            labelText: toolbar.showActionLabels ? toolbar.threadActionLabel() : ""
            toolTipText: toolbar.threadActionLabel()
            visible: toolbar.canSendText

            onClicked: {
                roomModel.thread = (toolbar.messageModel.threadId || toolbar.messageModel.eventId);
                toolbar.messageActionsControl.dismiss();
            }
        }

        MessageActionsToolbarButton {
            toolbarRef: toolbar
            image: ":/icons/icons/ui/reply.svg"
            labelText: toolbar.showActionLabels ? qsTr("Reply") : ""
            toolTipText: qsTr("Reply")
            visible: toolbar.canSendText

            onClicked: {
                roomModel.reply = toolbar.messageModel.eventId;
                toolbar.messageActionsControl.dismiss();
            }
        }

        MessageActionsToolbarButton {
            toolbarRef: toolbar
            image: ":/icons/icons/ui/reply.svg"
            labelText: toolbar.showActionLabels ? qsTr("Forward") : ""
            toolTipText: qsTr("Forward")
            mirrorIcon: true
            visible: toolbar.canForward

            onClicked: {
                toolbar.chatRoot.openForwardDialog(toolbar.messageModel.eventId);
                toolbar.messageActionsControl.dismiss();
            }
        }

        MessageActionsToolbarButton {
            toolbarRef: toolbar
            image: ":/icons/icons/ui/go-to.svg"
            labelText: toolbar.showActionLabels ? qsTr("Go to message") : ""
            toolTipText: qsTr("Go to message")
            visible: toolbar.canGoToMessage

            onClicked: {
                topBar.searchString = "";
                roomModel.showEvent(toolbar.messageModel.eventId);
                toolbar.messageActionsControl.dismiss();
            }
        }

        MessageActionsToolbarButton {
            id: optionsButton

            toolbarRef: toolbar
            image: ":/icons/icons/ui/options-circle.svg"
            labelText: ""
            toolTipText: qsTr("Options")

            onClicked: {
                if (!toolbar.messageModel)
                    return;

                toolbar.chatRoot.openMessageActionsDialog(
                    toolbar.messageModel.eventId,
                    toolbar.messageModel.threadId,
                    toolbar.messageModel.type,
                    toolbar.messageModel.isSender,
                    toolbar.messageModel.isEncrypted,
                    toolbar.messageModel.isEditable,
                    "",
                    toolbar.messageModel.body);
                toolbar.messageActionsControl.dismiss();
            }
        }
    }
}
