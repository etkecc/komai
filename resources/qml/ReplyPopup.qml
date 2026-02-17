// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "./delegates/"
import QtQuick 2.9
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.2
import im.nheko 1.0

Rectangle {
    id: replyPopup

    property color threadColor: room ? TimelineManager.userColor(room.thread, palette.base) : palette.buttonText

    Layout.fillWidth: true
    color: palette.alternateBase
    radius: 8
    implicitHeight: room && (room.reply || room.thread) ? popupColumn.implicitHeight + Nheko.paddingMedium * 2 : (room && room.edit ? closeEditButton.height + Nheko.paddingSmall : 0)
    visible: room && (room.reply || room.edit || room.thread)
    z: 3

    // Mask the bottom rounded corners so the popup sits flush against
    // the message input below. Only the top corners are rounded.
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: parent.radius
        color: parent.color
    }

    Column {
        id: popupColumn

        visible: room && (room.reply || room.thread)
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: Nheko.paddingMedium
        spacing: Nheko.paddingSmall

        // ── Thread header (visible when in a thread) ──
        RowLayout {
            visible: room && room.thread
            spacing: Nheko.paddingSmall
            width: parent.width

            Image {
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredHeight: threadHeaderLabel.font.pixelSize
                Layout.preferredWidth: threadHeaderLabel.font.pixelSize
                source: "image://colorimage/:/icons/icons/ui/thread.svg?" + replyPopup.threadColor
            }

            Label {
                id: threadHeaderLabel

                color: palette.text
                font.bold: true
                text: qsTr("Replying in a thread")
            }

            Item {
                Layout.fillWidth: true
            }

            ImageButton {
                id: closeThreadButton

                ToolTip.delay: Nheko.tooltipDelay
                ToolTip.text: qsTr("Close")
                ToolTip.visible: hovered
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredHeight: threadHeaderLabel.font.pixelSize
                Layout.preferredWidth: threadHeaderLabel.font.pixelSize
                hoverEnabled: true
                image: ":/icons/icons/ui/dismiss.svg"

                onClicked: room.thread = undefined
            }
        }

        // ── Reply header (visible when replying to a specific message) ──
        RowLayout {
            visible: room && room.reply
            spacing: Nheko.paddingSmall
            width: parent.width

            Image {
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredHeight: replyHeaderLabel.font.pixelSize
                Layout.preferredWidth: replyHeaderLabel.font.pixelSize
                source: "image://colorimage/:/icons/icons/ui/reply.svg?" + palette.text
            }

            Label {
                id: replyHeaderLabel

                color: palette.text
                font.bold: true
                text: qsTr("Replying to this message")
            }

            Item {
                Layout.fillWidth: true
            }

            ImageButton {
                id: closeReplyButton

                ToolTip.delay: Nheko.tooltipDelay
                ToolTip.text: qsTr("Close")
                ToolTip.visible: hovered
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredHeight: replyHeaderLabel.font.pixelSize
                Layout.preferredWidth: replyHeaderLabel.font.pixelSize
                hoverEnabled: true
                image: ":/icons/icons/ui/dismiss.svg"

                onClicked: room.reply = undefined
            }
        }

        // ── Reply preview (visible when replying to a specific message) ──
        Reply {
            id: replyPreview

            visible: room && room.reply

            property var modelData: room ? room.getDump(room.reply, room.id) : {}

            width: parent.width
            eventId: room?.reply ?? ""
            userColor: TimelineManager.userColor(modelData.userId, palette.window)
            maxWidth: parent.width
            limitHeight: true
        }
    }
    ImageButton {
        id: closeEditButton

        ToolTip.text: qsTr("Cancel Edit")
        ToolTip.visible: closeEditButton.hovered
        anchors.margins: 8
        anchors.right: parent.right
        anchors.top: parent.top
        height: 22
        hoverEnabled: true
        image: ":/icons/icons/ui/dismiss_edit.svg"
        visible: room && room.edit
        width: 22

        onClicked: room.edit = undefined
    }
}
