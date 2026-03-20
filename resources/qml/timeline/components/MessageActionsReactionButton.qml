// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import cc.etke.komai

AbstractButton {
    id: button

    property var reaction
    required property var messageModel
    required property var roomModel
    required property var messageActionsControl
    required property color actionButtonColor
    required property color actionButtonActiveColor
    required property color actionButtonHoverBackgroundColor
    required property int actionButtonIconSize
    required property int actionButtonHeight
    required property int itemHorizontalPadding
    required property int itemVerticalPadding
    readonly property string normalizedReaction: reaction !== undefined && reaction !== null ? String(reaction) : ""
    readonly property bool showImage: normalizedReaction.startsWith("mxc://")
    readonly property bool activeState: hovered || pressed || visualFocus

    activeFocusOnTab: true
    focusPolicy: Qt.StrongFocus
    leftPadding: itemHorizontalPadding
    rightPadding: itemHorizontalPadding
    topPadding: itemVerticalPadding
    bottomPadding: itemVerticalPadding
    leftInset: 0
    rightInset: 0
    topInset: 0
    bottomInset: 0
    implicitHeight: Math.max(actionButtonHeight,
                             (showImage ? actionButtonIconSize : reactionLabel.implicitHeight) + topPadding + bottomPadding)
    height: implicitHeight
    implicitWidth: (showImage ? actionButtonIconSize : reactionLabel.implicitWidth) + 2 * itemHorizontalPadding
    width: implicitWidth

    onClicked: {
        if (!messageModel)
            return;
        if (!normalizedReaction)
            return;
        roomModel.input.reaction(messageModel.eventId, normalizedReaction);
        TimelineManager.focusMessageInput();
        messageActionsControl.dismiss();
    }

    Label {
        id: reactionLabel

        anchors.centerIn: parent
        color: button.activeState ? button.actionButtonActiveColor : button.actionButtonColor
        font.pixelSize: button.actionButtonIconSize
        font.family: Settings.uiFontEmojiFamily
        horizontalAlignment: Text.AlignHCenter
        padding: 0
        text: TimelineManager.htmlEscape(button.normalizedReaction)
        verticalAlignment: Text.AlignVCenter
        visible: !button.showImage
    }
    Image {
        anchors.centerIn: parent
        width: button.actionButtonIconSize
        height: button.actionButtonIconSize
        fillMode: Image.PreserveAspectFit
        source: button.showImage ? (button.normalizedReaction.replace("mxc://", "image://MxcImage/") + "?scale") : ""
        sourceSize.height: height
        sourceSize.width: width
    }
    KomaiCursorShape {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
    }
    background: Rectangle {
        radius: Komai.paddingSmall
        color: button.activeState
            ? button.actionButtonHoverBackgroundColor
            : "transparent"
    }
}
