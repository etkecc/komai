// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../delegates/"
import QtQuick 2.9
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.2
import "../components"
import cc.etke.komai 1.0

Rectangle {
    id: replyPopup

    property bool roundTopCorners: true
    property color threadColor: room ? TimelineManager.userColor(room.thread, palette.base) : palette.buttonText
    readonly property bool layoutVisible: room && (room.reply || room.thread || room.edit)
    property int headerTextHeight: Math.round(Komai.fontPixelSize * 2.4)
    property int headerIconSize: Math.ceil(replyPopup.headerTextHeight * 0.5)
    property int headerFontSize: Math.ceil(replyPopup.headerTextHeight * 0.45)

    Layout.fillWidth: true
    Layout.minimumHeight: 0
    Layout.preferredHeight: layoutVisible ? implicitHeight : 0
    Layout.maximumHeight: layoutVisible ? implicitHeight : 0
    color: palette.alternateBase
    radius: replyPopup.roundTopCorners ? 8 : 0
    implicitHeight: layoutVisible ? popupColumn.implicitHeight + Komai.paddingMedium * 2 : 0
    visible: layoutVisible
    z: 3

    // Mask the bottom rounded corners so the popup sits flush against
    // the message input below. Only the top corners are rounded.
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: parent.radius
        color: parent.color
        visible: replyPopup.roundTopCorners
    }

    Column {
        id: popupColumn

        visible: replyPopup.layoutVisible
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: Komai.paddingMedium
        spacing: Komai.paddingSmall

        // ── Thread header (visible when in a thread) ──
        RowLayout {
            visible: room && room.thread
            spacing: Komai.paddingSmall
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

                ToolTip.delay: Komai.tooltipDelay
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
            spacing: Komai.paddingSmall
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
                text: replyPreview.replyDisplayName !== ""
                    ? qsTr("Replying to %1").arg(replyPreview.replyDisplayName)
                    : qsTr("Replying to this message")
            }

            Item {
                Layout.fillWidth: true
            }

            ImageButton {
                id: closeReplyButton

                ToolTip.delay: Komai.tooltipDelay
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
            spacing: Komai.paddingSmall
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

                ToolTip.delay: Komai.tooltipDelay
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
            property string replyUserId: (modelData && modelData.userId)
                ? String(modelData.userId)
                : ""
            property string replyDisplayName: {
                const userName = (modelData && modelData.userName)
                        ? String(modelData.userName).trim()
                        : "";
                if (userName.length > 0)
                    return userName;
                return replyUserId;
            }
            property bool isReplyFromCurrentUser: {
                const currentUser = Komai.currentUser;
                const currentUserId = (currentUser && currentUser.userid)
                        ? String(currentUser.userid)
                        : "";
                return currentUserId.length > 0 && replyUserId === currentUserId;
            }
            readonly property color previewWindowColor: (Komai.colors && Komai.colors.window !== undefined)
                ? Komai.colors.window
                : replyPopup.palette.window
            readonly property color previewBaseColor: (Komai.colors && Komai.colors.base !== undefined)
                ? Komai.colors.base
                : replyPopup.palette.base

            width: parent.width
            eventId: room?.reply ?? ""
            bubblePalette: room ? TimelineManager.roomUserBubblePalette(room.roomId, replyUserId, roomColor, Settings.timelineUserColorCodingPolicy) : TimelineManager.userBubblePalette(replyUserId, roomColor)
            userColor: isReplyFromCurrentUser
                ? Komai.theme.userColorSelf
                : room ? TimelineManager.roomUserColor(room.roomId, replyUserId, previewWindowColor, Settings.timelineUserColorCodingPolicy) : TimelineManager.userColor(replyUserId, previewWindowColor)
            roomColor: isReplyFromCurrentUser
                ? Komai.theme.userColorSelf
                : room ? TimelineManager.roomUserColor(room.roomId, replyUserId, previewBaseColor, Settings.timelineUserColorCodingPolicy) : TimelineManager.userColor(replyUserId, previewBaseColor)
            maxWidth: parent.width
            limitHeight: true
        }
    }

}
