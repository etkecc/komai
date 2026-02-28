// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components"
import QtQuick
import QtQuick.Controls
import im.nheko

AbstractButton {
    id: button

    required property string reaction
    required property var messageModel
    required property var roomModel
    required property var messageActionsControl
    required property color actionButtonColor
    required property color actionButtonHoverBackgroundColor
    required property int actionButtonIconSize
    required property int actionButtonHeight
    required property int itemHorizontalPadding
    required property int itemVerticalPadding
    readonly property bool showImage: reaction.startsWith("mxc://")

    focusPolicy: Qt.NoFocus
    leftPadding: itemHorizontalPadding
    rightPadding: itemHorizontalPadding
    topPadding: itemVerticalPadding
    bottomPadding: itemVerticalPadding
    leftInset: 0
    rightInset: 0
    topInset: 0
    bottomInset: 0
    height: actionButtonHeight
    implicitHeight: actionButtonHeight
    implicitWidth: (showImage ? actionButtonIconSize : reactionText.implicitWidth) + 2 * itemHorizontalPadding
    width: implicitWidth

    onClicked: {
        if (!messageModel)
            return;
        roomModel.input.reaction(messageModel.eventId, reaction);
        TimelineManager.focusMessageInput();
        messageActionsControl.dismiss();
    }

    Label {
        id: reactionText

        anchors.centerIn: parent
        color: button.actionButtonColor
        font.pixelSize: button.actionButtonIconSize
        font.family: Settings.uiFontEmojiFamily
        horizontalAlignment: Text.AlignHCenter
        padding: 0
        text: TimelineManager.htmlEscape(button.reaction)
        verticalAlignment: Text.AlignVCenter
        visible: !button.showImage
    }
    Image {
        anchors.centerIn: parent
        width: button.actionButtonIconSize
        height: button.actionButtonIconSize
        fillMode: Image.PreserveAspectFit
        source: button.showImage ? (button.reaction.replace("mxc://", "image://MxcImage/") + "?scale") : ""
        sourceSize.height: height
        sourceSize.width: width
    }
    NhekoCursorShape {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
    }
    background: Rectangle {
        radius: Nheko.paddingMedium
        color: button.hovered || button.pressed || button.visualFocus
            ? button.actionButtonHoverBackgroundColor
            : "transparent"
    }
}
