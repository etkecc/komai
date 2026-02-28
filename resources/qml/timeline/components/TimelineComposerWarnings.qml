// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../composer" as Composer
import QtQuick
import QtQuick.Layouts
import im.nheko

ColumnLayout {
    id: root

    required property var roomModel
    required property bool replyPopupVisible

    readonly property int mentionCount: roomModel ? roomModel.input.mentions.length : 0

    Layout.fillWidth: true
    spacing: 0

    Repeater {
        model: root.roomModel ? root.roomModel.input.mentions : null

        delegate: TimelineMentionWarningBar {
            mention: modelData
            mentionIndex: index
            replyPopupVisible: root.replyPopupVisible
            room: root.roomModel
        }
    }

    Composer.MessageInputWarning {
        roundTopCorners: !root.replyPopupVisible && root.mentionCount == 0
        text: qsTr("The command /%1 is not recognized and will be sent as part of your message").arg(root.roomModel ? root.roomModel.input.currentCommand : "")
        visible: root.roomModel ? root.roomModel.input.containsInvalidCommand && !root.roomModel.input.containsIncompleteCommand : false
    }

    Composer.MessageInputWarning {
        roundTopCorners: !root.replyPopupVisible && root.mentionCount == 0
        bubbleColor: Nheko.theme.orange
        text: qsTr("/%1 looks like an incomplete command. To send it anyway, add a space to the end of your message.").arg(root.roomModel ? root.roomModel.input.currentCommand : "")
        visible: root.roomModel ? root.roomModel.input.containsIncompleteCommand : false
    }
}
