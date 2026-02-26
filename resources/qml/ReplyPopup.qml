// SPDX-FileCopyrightText: Nheko Contributors
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
    property int headerTextHeight: Math.round(Qt.application.font.pixelSize * 2.4)
    property int headerIconSize: Math.ceil(replyPopup.headerTextHeight * 0.5)
    property int headerFontSize: Math.ceil(replyPopup.headerTextHeight * 0.45)

    Layout.fillWidth: true
    color: palette.alternateBase
    radius: 8
    implicitHeight: room && (room.reply || room.thread || room.edit) ? popupColumn.implicitHeight + Nheko.paddingMedium * 2 : 0
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

        visible: room && (room.reply || room.thread || room.edit)
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
                Layout.preferredHeight: replyPopup.headerIconSize
                Layout.preferredWidth: replyPopup.headerIconSize
                source: "image://colorimage/:/icons/icons/ui/thread.svg?" + replyPopup.threadColor
            }

            Label {
                id: threadHeaderLabel

                color: palette.text
                font.pixelSize: replyPopup.headerFontSize
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
                Layout.preferredHeight: replyPopup.headerIconSize
                Layout.preferredWidth: replyPopup.headerIconSize
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
                Layout.preferredHeight: replyPopup.headerIconSize
                Layout.preferredWidth: replyPopup.headerIconSize
                source: "image://colorimage/:/icons/icons/ui/reply.svg?" + palette.text
            }

            Label {
                id: replyHeaderLabel

                color: palette.text
                font.pixelSize: replyPopup.headerFontSize
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
                Layout.preferredHeight: replyPopup.headerIconSize
                Layout.preferredWidth: replyPopup.headerIconSize
                hoverEnabled: true
                image: ":/icons/icons/ui/dismiss.svg"

                onClicked: room.reply = undefined
            }
        }

        // ── Edit header (visible when editing a message) ──
        RowLayout {
            visible: room && room.edit
            spacing: Nheko.paddingSmall
            width: parent.width

            Image {
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredHeight: replyPopup.headerIconSize
                Layout.preferredWidth: replyPopup.headerIconSize
                source: "image://colorimage/:/icons/icons/ui/edit.svg?" + palette.text
            }

            Label {
                id: editHeaderLabel

                color: palette.text
                font.pixelSize: replyPopup.headerFontSize
                font.bold: true
                text: qsTr("Editing a message")
            }

            Item {
                Layout.fillWidth: true
            }

            ImageButton {
                id: closeEditHeaderButton

                ToolTip.delay: Nheko.tooltipDelay
                ToolTip.text: qsTr("Close")
                ToolTip.visible: hovered
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredHeight: replyPopup.headerIconSize
                Layout.preferredWidth: replyPopup.headerIconSize
                hoverEnabled: true
                image: ":/icons/icons/ui/dismiss.svg"

                onClicked: room.edit = undefined
            }
        }

        // ── Reply preview (visible when replying to a specific message) ──
        Reply {
            id: replyPreview

            visible: room && room.reply

            property var modelData: room ? room.getDump(room.reply, room.id) : {}

            width: parent.width
            eventId: room?.reply ?? ""
            userColor: room ? TimelineManager.roomUserColor(room.roomId, modelData.userId, palette.window, palette.highlight) : TimelineManager.userColor(modelData.userId, palette.window)
            roomColor: room ? TimelineManager.roomUserColor(room.roomId, modelData.userId, palette.base, palette.highlight) : TimelineManager.userColor(modelData.userId, palette.base)
            maxWidth: parent.width
            limitHeight: true
        }
    }

}
