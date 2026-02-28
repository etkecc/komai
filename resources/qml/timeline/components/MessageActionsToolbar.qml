// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import im.nheko

RowLayout {
    id: toolbar

    required property var chatRoot
    required property var emojiPopup
    required property var filteredTimeline
    required property var messageActionsControl
    required property var messageContextMenu
    required property var messageModel
    required property var roomModel
    required property var topBar
    property int itemHorizontalPadding: Nheko.paddingMedium
    property int itemVerticalPadding: Nheko.paddingSmall
    property int actionButtonIconSize: 32
    property int actionButtonHeight: actionButtonIconSize + itemVerticalPadding * 2
    property int labelBreakpointWidth: 600
    readonly property real actionHostWidth: (messageActionsControl && messageActionsControl.parent)
        ? messageActionsControl.parent.width
        : (chatRoot ? chatRoot.width : width)
    readonly property bool canReact: roomModel ? roomModel.permissions.canSend(MtxEvent.Reaction) : false
    readonly property bool canSendText: roomModel ? roomModel.permissions.canSend(MtxEvent.TextMessage) : false
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
    readonly property bool showActionLabels: actionHostWidth >= labelBreakpointWidth
        && actionHostWidth >= requiredLabeledWidth
    readonly property color actionBarColor: Qt.rgba(Qt.darker(palette.base, 2.1).r, Qt.darker(palette.base, 2.1).g, Qt.darker(palette.base, 2.1).b, 0.88)
    property color actionButtonColor: palette.brightText
    property color actionButtonHoverColor: palette.highlight
    property color actionButtonHoverBackgroundColor: Qt.rgba(actionButtonColor.r, actionButtonColor.g, actionButtonColor.b, 0.16)
    readonly property int separatorSlotWidth: Nheko.paddingMedium * 2 + 1
    readonly property real reactionButtonsWidth: repeaterItemsWidth(pinnedReactionsRepeater) + repeaterItemsWidth(recentReactionsRepeater)

    spacing: 0

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
        return iconOnlyButtonWidth() + Nheko.paddingSmall + labelAdvanceWidth;
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

    Repeater {
        id: pinnedReactionsRepeater

        model: Settings.timelineMessageActionsPinnedReactions.split(",").map(function (s) {
            return s.trim();
        }).filter(function (s) {
            return s.length > 0;
        }).slice(0, 10)
        visible: toolbar.canReact

        delegate: AbstractButton {
            id: pinnedReactionButton

            property color buttonTextColor: toolbar.actionButtonColor
            property color highlightColor: toolbar.actionButtonHoverColor
            required property string modelData
            property bool showImage: modelData.startsWith("mxc://")

            focusPolicy: Qt.NoFocus
            leftPadding: toolbar.itemHorizontalPadding
            rightPadding: toolbar.itemHorizontalPadding
            topPadding: toolbar.itemVerticalPadding
            bottomPadding: toolbar.itemVerticalPadding
            leftInset: 0
            rightInset: 0
            topInset: 0
            bottomInset: 0
            height: toolbar.actionButtonHeight
            implicitHeight: toolbar.actionButtonHeight
            implicitWidth: (showImage ? toolbar.actionButtonIconSize : pinnedReactionText.implicitWidth) + 2 * toolbar.itemHorizontalPadding
            width: implicitWidth

            onClicked: {
                if (!toolbar.messageModel)
                    return;
                toolbar.roomModel.input.reaction(toolbar.messageModel.eventId, modelData);
                TimelineManager.focusMessageInput();
                toolbar.messageActionsControl.dismiss();
            }

            Label {
                id: pinnedReactionText

                anchors.centerIn: parent
                color: pinnedReactionButton.buttonTextColor
                font.pixelSize: toolbar.actionButtonIconSize
                font.family: Settings.uiFontEmojiFamily
                horizontalAlignment: Text.AlignHCenter
                padding: 0
                text: TimelineManager.htmlEscape(pinnedReactionButton.modelData)
                verticalAlignment: Text.AlignVCenter
                visible: !pinnedReactionButton.showImage
            }
            Image {
                anchors.centerIn: parent
                width: toolbar.actionButtonIconSize
                height: toolbar.actionButtonIconSize
                fillMode: Image.PreserveAspectFit
                source: pinnedReactionButton.showImage ? (pinnedReactionButton.modelData.replace("mxc://", "image://MxcImage/") + "?scale") : ""
                sourceSize.height: height
                sourceSize.width: width
            }
            NhekoCursorShape {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
            }
            background: Rectangle {
                radius: Nheko.paddingMedium
                color: pinnedReactionButton.hovered || pinnedReactionButton.pressed || pinnedReactionButton.visualFocus
                    ? toolbar.actionButtonHoverBackgroundColor
                    : "transparent"
            }
        }
    }

    Repeater {
        id: recentReactionsRepeater

        property var pinnedSet: Settings.timelineMessageActionsPinnedReactions.split(",").map(function (s) {
            return s.trim();
        }).filter(function (s) {
            return s.length > 0;
        }).slice(0, 10)
        model: Settings.recentReactions.filter(function (reaction) {
            return pinnedSet.indexOf(reaction) < 0;
        }).slice(0, Math.max(0, 10 - pinnedSet.length))
        visible: toolbar.canReact

        delegate: AbstractButton {
            id: recentReactionButton

            property color buttonTextColor: toolbar.actionButtonColor
            property color highlightColor: toolbar.actionButtonHoverColor
            required property string modelData
            property bool showImage: modelData.startsWith("mxc://")

            focusPolicy: Qt.NoFocus
            leftPadding: toolbar.itemHorizontalPadding
            rightPadding: toolbar.itemHorizontalPadding
            topPadding: toolbar.itemVerticalPadding
            bottomPadding: toolbar.itemVerticalPadding
            leftInset: 0
            rightInset: 0
            topInset: 0
            bottomInset: 0
            height: toolbar.actionButtonHeight
            implicitHeight: toolbar.actionButtonHeight
            implicitWidth: (showImage ? toolbar.actionButtonIconSize : recentReactionText.implicitWidth) + 2 * toolbar.itemHorizontalPadding
            width: implicitWidth

            onClicked: {
                if (!toolbar.messageModel)
                    return;
                toolbar.roomModel.input.reaction(toolbar.messageModel.eventId, modelData);
                TimelineManager.focusMessageInput();
                toolbar.messageActionsControl.dismiss();
            }

            Label {
                id: recentReactionText

                anchors.centerIn: parent
                color: recentReactionButton.buttonTextColor
                font.pixelSize: toolbar.actionButtonIconSize
                font.family: Settings.uiFontEmojiFamily
                horizontalAlignment: Text.AlignHCenter
                padding: 0
                text: TimelineManager.htmlEscape(recentReactionButton.modelData)
                verticalAlignment: Text.AlignVCenter
                visible: !recentReactionButton.showImage
            }
            Image {
                anchors.centerIn: parent
                width: toolbar.actionButtonIconSize
                height: toolbar.actionButtonIconSize
                fillMode: Image.PreserveAspectFit
                source: recentReactionButton.showImage ? (recentReactionButton.modelData.replace("mxc://", "image://MxcImage/") + "?scale") : ""
                sourceSize.height: height
                sourceSize.width: width
            }
            NhekoCursorShape {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
            }
            background: Rectangle {
                radius: Nheko.paddingMedium
                color: recentReactionButton.hovered || recentReactionButton.pressed || recentReactionButton.visualFocus
                    ? toolbar.actionButtonHoverBackgroundColor
                    : "transparent"
            }
        }
    }

    MessageActionsLabeledButton {
        id: reactButton

        buttonTextColor: toolbar.actionButtonColor
        hoverIconColor: toolbar.actionButtonHoverColor
        hoverTextColor: toolbar.actionButtonHoverColor
        hoverBackgroundColor: toolbar.actionButtonHoverBackgroundColor
        iconSize: toolbar.actionButtonIconSize
        image: ":/icons/icons/ui/smile-add.svg"
        labelText: ""
        toolTipText: qsTr("React")
        visible: toolbar.canReact
        contentHorizontalPadding: toolbar.itemHorizontalPadding
        contentVerticalPadding: toolbar.itemVerticalPadding

        onClicked: emojiPopup.visible ? emojiPopup.close() : emojiPopup.show(reactButton, roomModel.roomId, function (plaintext, markdown) {
                var eventId = toolbar.messageModel ? toolbar.messageModel.eventId : "";
                roomModel.input.reaction(eventId, plaintext);
                TimelineManager.focusMessageInput();
            })
    }
    Item {
        Layout.preferredWidth: Nheko.paddingMedium * 2 + separator.width
        Layout.preferredHeight: toolbar.actionButtonHeight
        visible: reactButton.visible

        Rectangle {
            id: separator

            anchors.verticalCenter: parent.verticalCenter
            anchors.horizontalCenter: parent.horizontalCenter
            width: 1
            height: Math.max(1, parent.height - Nheko.paddingMedium)
            color: Qt.rgba(toolbar.actionButtonColor.r, toolbar.actionButtonColor.g, toolbar.actionButtonColor.b, 0.35)
        }
    }

    MessageActionsLabeledButton {
        buttonTextColor: toolbar.actionButtonColor
        hoverIconColor: toolbar.actionButtonHoverColor
        hoverTextColor: toolbar.actionButtonHoverColor
        hoverBackgroundColor: toolbar.actionButtonHoverBackgroundColor
        iconSize: toolbar.actionButtonIconSize
        image: ":/icons/icons/ui/edit.svg"
        contentHorizontalPadding: toolbar.itemHorizontalPadding
        contentVerticalPadding: toolbar.itemVerticalPadding
        labelText: toolbar.showActionLabels ? qsTr("Edit") : ""
        toolTipText: qsTr("Edit")
        visible: toolbar.canEdit

        onClicked: {
            if (toolbar.messageModel.isEditable)
                roomModel.edit = toolbar.messageModel.eventId;
            toolbar.messageActionsControl.dismiss();
        }
    }

    MessageActionsLabeledButton {
        buttonTextColor: toolbar.actionButtonColor
        hoverIconColor: toolbar.actionButtonHoverColor
        hoverTextColor: toolbar.actionButtonHoverColor
        hoverBackgroundColor: toolbar.actionButtonHoverBackgroundColor
        iconSize: toolbar.actionButtonIconSize
        image: ":/icons/icons/ui/thread.svg"
        contentHorizontalPadding: toolbar.itemHorizontalPadding
        contentVerticalPadding: toolbar.itemVerticalPadding
        labelText: toolbar.showActionLabels ? toolbar.threadActionLabel() : ""
        toolTipText: toolbar.threadActionLabel()
        visible: toolbar.canSendText

        onClicked: {
            roomModel.thread = (toolbar.messageModel.threadId || toolbar.messageModel.eventId);
            toolbar.messageActionsControl.dismiss();
        }
    }

    MessageActionsLabeledButton {
        buttonTextColor: toolbar.actionButtonColor
        hoverIconColor: toolbar.actionButtonHoverColor
        hoverTextColor: toolbar.actionButtonHoverColor
        hoverBackgroundColor: toolbar.actionButtonHoverBackgroundColor
        iconSize: toolbar.actionButtonIconSize
        image: ":/icons/icons/ui/reply.svg"
        contentHorizontalPadding: toolbar.itemHorizontalPadding
        contentVerticalPadding: toolbar.itemVerticalPadding
        labelText: toolbar.showActionLabels ? qsTr("Reply") : ""
        toolTipText: qsTr("Reply")
        visible: toolbar.canSendText

        onClicked: {
            roomModel.reply = toolbar.messageModel.eventId;
            toolbar.messageActionsControl.dismiss();
        }
    }

    MessageActionsLabeledButton {
        buttonTextColor: toolbar.actionButtonColor
        hoverIconColor: toolbar.actionButtonHoverColor
        hoverTextColor: toolbar.actionButtonHoverColor
        hoverBackgroundColor: toolbar.actionButtonHoverBackgroundColor
        iconSize: toolbar.actionButtonIconSize
        image: ":/icons/icons/ui/reply.svg"
        contentHorizontalPadding: toolbar.itemHorizontalPadding
        contentVerticalPadding: toolbar.itemVerticalPadding
        labelText: toolbar.showActionLabels ? qsTr("Forward") : ""
        toolTipText: qsTr("Forward")
        mirrorIcon: true
        visible: toolbar.canForward

        onClicked: {
            toolbar.chatRoot.openForwardDialog(toolbar.messageModel.eventId);
            toolbar.messageActionsControl.dismiss();
        }
    }

    MessageActionsLabeledButton {
        buttonTextColor: toolbar.actionButtonColor
        hoverIconColor: toolbar.actionButtonHoverColor
        hoverTextColor: toolbar.actionButtonHoverColor
        hoverBackgroundColor: toolbar.actionButtonHoverBackgroundColor
        iconSize: toolbar.actionButtonIconSize
        image: ":/icons/icons/ui/go-to.svg"
        contentHorizontalPadding: toolbar.itemHorizontalPadding
        contentVerticalPadding: toolbar.itemVerticalPadding
        labelText: toolbar.showActionLabels ? qsTr("Go to message") : ""
        toolTipText: qsTr("Go to message")
        visible: toolbar.canGoToMessage

        onClicked: {
            topBar.searchString = "";
            roomModel.showEvent(toolbar.messageModel.eventId);
            toolbar.messageActionsControl.dismiss();
        }
    }

    MessageActionsLabeledButton {
        id: optionsButton

        buttonTextColor: toolbar.actionButtonColor
        hoverIconColor: toolbar.actionButtonHoverColor
        hoverTextColor: toolbar.actionButtonHoverColor
        hoverBackgroundColor: toolbar.actionButtonHoverBackgroundColor
        iconSize: toolbar.actionButtonIconSize
        image: ":/icons/icons/ui/options-circle.svg"
        labelText: ""
        toolTipText: qsTr("Options")
        contentHorizontalPadding: toolbar.itemHorizontalPadding
        contentVerticalPadding: toolbar.itemVerticalPadding

        onClicked: {
            if (!toolbar.messageModel)
                return;

            const sameTargetVisible = messageContextMenu.visible
                && messageContextMenu.eventId === toolbar.messageModel.eventId;

            if (sameTargetVisible) {
                messageContextMenu.close();
                return;
            }

            if (messageContextMenu.wasJustClosedFor(toolbar.messageModel.eventId, optionsButton))
                return;

            messageContextMenu.show(toolbar.messageModel.eventId,
                                    toolbar.messageModel.threadId,
                                    toolbar.messageModel.type,
                                    toolbar.messageModel.isSender,
                                    toolbar.messageModel.isEncrypted,
                                    toolbar.messageModel.isEditable,
                                    "",
                                    toolbar.messageModel.body,
                                    optionsButton);
        }
    }
}
