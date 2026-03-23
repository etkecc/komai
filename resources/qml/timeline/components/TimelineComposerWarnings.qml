// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../composer" as Composer
import QtQuick
import QtQuick.Layouts
import cc.etke.komai

ColumnLayout {
    id: root

    required property var roomModel
    required property bool commandPickerVisible
    required property bool replyPopupVisible

    readonly property int mentionCount: roomModel ? roomModel.input.mentions.length : 0
    readonly property string commandValidationState: roomModel ? roomModel.input.commandValidationState : "none"
    readonly property string commandValidationMessage: roomModel ? roomModel.input.commandValidationMessage : ""
    readonly property bool commandWarningVisible: !commandPickerVisible
        && commandValidationMessage.length > 0
    readonly property color commandWarningColor: commandValidationState === "incomplete"
        || commandValidationState === "unrecognized"
        ? Komai.theme.warning
        : Komai.theme.error
    readonly property bool layoutVisible: mentionCount > 0 || commandWarningVisible

    Layout.fillWidth: true
    Layout.minimumHeight: 0
    Layout.preferredHeight: layoutVisible ? implicitHeight : 0
    Layout.maximumHeight: layoutVisible ? implicitHeight : 0
    spacing: 0
    visible: layoutVisible

    Repeater {
        model: root.roomModel ? root.roomModel.input.mentions : null

        delegate: TimelineMentionWarningBar {
            required property string modelData
            required property int index

            mention: modelData
            mentionIndex: index
            replyPopupVisible: root.replyPopupVisible
            room: root.roomModel
        }
    }

    Composer.MessageInputWarning {
        roundTopCorners: !root.replyPopupVisible && root.mentionCount == 0
        bubbleColor: root.commandWarningColor
        text: root.commandValidationMessage
        visible: root.commandWarningVisible
    }
}
