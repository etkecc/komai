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
    property int itemPadding: Math.round(messageActionsControl.padding / 2)

    spacing: 0

    function isForwardableType(eventType) {
        return eventType == MtxEvent.ImageMessage || eventType == MtxEvent.VideoMessage || eventType == MtxEvent.AudioMessage || eventType == MtxEvent.FileMessage || eventType == MtxEvent.Sticker || eventType == MtxEvent.TextMessage || eventType == MtxEvent.LocationMessage || eventType == MtxEvent.EmoteMessage || eventType == MtxEvent.NoticeMessage;
    }

    Repeater {
        model: Settings.timelineMessageActionsPinnedReactions.split(",").map(function (s) {
            return s.trim();
        }).filter(function (s) {
            return s.length > 0;
        }).slice(0, 10)
        visible: roomModel ? roomModel.permissions.canSend(MtxEvent.Reaction) : false

        delegate: AbstractButton {
            id: pinnedReactionButton

            property color buttonTextColor: palette.buttonText
            property color highlightColor: palette.highlight
            required property string modelData
            property bool showImage: modelData.startsWith("mxc://")

            Layout.alignment: Qt.AlignBottom
            focusPolicy: Qt.NoFocus
            leftPadding: toolbar.itemPadding
            rightPadding: toolbar.itemPadding
            height: showImage ? 32 : pinnedReactionText.implicitHeight
            implicitHeight: showImage ? 32 : pinnedReactionText.implicitHeight
            implicitWidth: (showImage ? 32 : pinnedReactionText.implicitWidth) + 2 * toolbar.itemPadding
            width: (showImage ? 32 : pinnedReactionText.implicitWidth) + 2 * toolbar.itemPadding

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
                color: pinnedReactionButton.hovered ? pinnedReactionButton.highlightColor : pinnedReactionButton.buttonTextColor
                font.pixelSize: 32
                font.family: Settings.uiFontEmojiFamily
                horizontalAlignment: Text.AlignHCenter
                padding: 0
                text: TimelineManager.htmlEscape(pinnedReactionButton.modelData)
                verticalAlignment: Text.AlignVCenter
                visible: !pinnedReactionButton.showImage
            }
            Image {
                anchors.fill: parent
                fillMode: Image.PreserveAspectFit
                source: pinnedReactionButton.showImage ? (pinnedReactionButton.modelData.replace("mxc://", "image://MxcImage/") + "?scale") : ""
                sourceSize.height: pinnedReactionButton.height
                sourceSize.width: pinnedReactionButton.width
            }
            NhekoCursorShape {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
            }
            Ripple {
                color: Qt.rgba(pinnedReactionButton.buttonTextColor.r, pinnedReactionButton.buttonTextColor.g, pinnedReactionButton.buttonTextColor.b, 0.5)
            }
            HoverPulseAnimation {
                id: pinnedReactionPulse

                targetItem: pinnedReactionButton
            }
            onHoveredChanged: {
                if (hovered)
                    pinnedReactionPulse.pulse();
            }
        }
    }

    Repeater {
        property var pinnedSet: Settings.timelineMessageActionsPinnedReactions.split(",").map(function (s) {
            return s.trim();
        }).filter(function (s) {
            return s.length > 0;
        }).slice(0, 10)
        model: Settings.recentReactions.filter(function (reaction) {
            return pinnedSet.indexOf(reaction) < 0;
        }).slice(0, Math.max(0, 10 - pinnedSet.length))
        visible: roomModel ? roomModel.permissions.canSend(MtxEvent.Reaction) : false

        delegate: AbstractButton {
            id: recentReactionButton

            property color buttonTextColor: palette.buttonText
            property color highlightColor: palette.highlight
            required property string modelData
            property bool showImage: modelData.startsWith("mxc://")

            Layout.alignment: Qt.AlignBottom
            focusPolicy: Qt.NoFocus
            leftPadding: toolbar.itemPadding
            rightPadding: toolbar.itemPadding
            height: showImage ? 32 : recentReactionText.implicitHeight
            implicitHeight: showImage ? 32 : recentReactionText.implicitHeight
            implicitWidth: (showImage ? 32 : recentReactionText.implicitWidth) + 2 * toolbar.itemPadding
            width: (showImage ? 32 : recentReactionText.implicitWidth) + 2 * toolbar.itemPadding

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
                color: recentReactionButton.hovered ? recentReactionButton.highlightColor : recentReactionButton.buttonTextColor
                font.pixelSize: 32
                font.family: Settings.uiFontEmojiFamily
                horizontalAlignment: Text.AlignHCenter
                padding: 0
                text: TimelineManager.htmlEscape(recentReactionButton.modelData)
                verticalAlignment: Text.AlignVCenter
                visible: !recentReactionButton.showImage
            }
            Image {
                anchors.fill: parent
                fillMode: Image.PreserveAspectFit
                source: recentReactionButton.showImage ? (recentReactionButton.modelData.replace("mxc://", "image://MxcImage/") + "?scale") : ""
                sourceSize.height: recentReactionButton.height
                sourceSize.width: recentReactionButton.width
            }
            NhekoCursorShape {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
            }
            Ripple {
                color: Qt.rgba(recentReactionButton.buttonTextColor.r, recentReactionButton.buttonTextColor.g, recentReactionButton.buttonTextColor.b, 0.5)
            }
            HoverPulseAnimation {
                id: recentReactionPulse

                targetItem: recentReactionButton
            }
            onHoveredChanged: {
                if (hovered)
                    recentReactionPulse.pulse();
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
        visible: roomModel ? roomModel.permissions.canSend(MtxEvent.Reaction) : false
        leftPadding: toolbar.itemPadding
        rightPadding: toolbar.itemPadding
        Layout.preferredWidth: 32 + 2 * toolbar.itemPadding
        Layout.preferredHeight: 32

        onClicked: emojiPopup.visible ? emojiPopup.close() : emojiPopup.show(reactButton, roomModel.roomId, function (plaintext, markdown) {
                var eventId = toolbar.messageModel ? toolbar.messageModel.eventId : "";
                roomModel.input.reaction(eventId, plaintext);
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
        visible: !!toolbar.messageModel && toolbar.messageModel.isEditable
        leftPadding: toolbar.itemPadding
        rightPadding: toolbar.itemPadding
        Layout.preferredWidth: 32 + 2 * toolbar.itemPadding
        Layout.preferredHeight: 32

        onClicked: {
            if (toolbar.messageModel.isEditable)
                roomModel.edit = toolbar.messageModel.eventId;
            toolbar.messageActionsControl.dismiss();
        }
    }

    ImageButton {
        ToolTip.delay: Nheko.tooltipDelay
        ToolTip.text: (toolbar.messageModel && toolbar.messageModel.threadId) ? qsTr("Reply in thread") : qsTr("New thread")
        ToolTip.visible: hovered
        hoverEnabled: true
        hoverPulse: true
        image: ":/icons/icons/ui/thread.svg"
        visible: roomModel ? roomModel.permissions.canSend(MtxEvent.TextMessage) : false
        leftPadding: toolbar.itemPadding
        rightPadding: toolbar.itemPadding
        Layout.preferredWidth: 32 + 2 * toolbar.itemPadding
        Layout.preferredHeight: 32

        onClicked: {
            roomModel.thread = (toolbar.messageModel.threadId || toolbar.messageModel.eventId);
            toolbar.messageActionsControl.dismiss();
        }
    }

    ImageButton {
        ToolTip.delay: Nheko.tooltipDelay
        ToolTip.text: qsTr("Reply")
        ToolTip.visible: hovered
        hoverEnabled: true
        hoverPulse: true
        image: ":/icons/icons/ui/reply.svg"
        visible: roomModel ? roomModel.permissions.canSend(MtxEvent.TextMessage) : false
        leftPadding: toolbar.itemPadding
        rightPadding: toolbar.itemPadding
        Layout.preferredWidth: 32 + 2 * toolbar.itemPadding
        Layout.preferredHeight: 32

        onClicked: {
            roomModel.reply = toolbar.messageModel.eventId;
            toolbar.messageActionsControl.dismiss();
        }
    }

    ImageButton {
        ToolTip.delay: Nheko.tooltipDelay
        ToolTip.text: qsTr("Forward")
        ToolTip.visible: hovered
        hoverEnabled: true
        hoverPulse: true
        image: ":/icons/icons/ui/reply.svg"
        visible: !!toolbar.messageModel && toolbar.isForwardableType(toolbar.messageModel.type)
        leftPadding: toolbar.itemPadding
        rightPadding: toolbar.itemPadding
        Layout.preferredWidth: 32 + 2 * toolbar.itemPadding
        Layout.preferredHeight: 32
        transform: Scale {
            origin.x: 16 + toolbar.itemPadding
            xScale: -1
        }

        onClicked: {
            toolbar.chatRoot.openForwardDialog(toolbar.messageModel.eventId);
            toolbar.messageActionsControl.dismiss();
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
        visible: !!toolbar.messageModel && filteredTimeline.filterByContent
        leftPadding: toolbar.itemPadding
        rightPadding: toolbar.itemPadding
        Layout.preferredWidth: 32 + 2 * toolbar.itemPadding
        Layout.preferredHeight: 32

        onClicked: {
            topBar.searchString = "";
            roomModel.showEvent(toolbar.messageModel.eventId);
            toolbar.messageActionsControl.dismiss();
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
        leftPadding: toolbar.itemPadding
        rightPadding: toolbar.itemPadding
        Layout.preferredWidth: 32 + 2 * toolbar.itemPadding
        Layout.preferredHeight: 32

        onClicked: messageContextMenu.show(toolbar.messageModel.eventId, toolbar.messageModel.threadId, toolbar.messageModel.type, toolbar.messageModel.isSender, toolbar.messageModel.isEncrypted, toolbar.messageModel.isEditable, "", toolbar.messageModel.body, optionsButton)
    }
}
