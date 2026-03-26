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
    property var roomModel: (typeof room !== "undefined") ? room : null
    property string matrixReplyEventId: ""
    property string matrixReplySenderId: ""
    property string matrixReplyDisplayName: ""
    property string matrixReplyBody: ""
    property string matrixEditEventId: ""
    readonly property bool matrixReplyMode: matrixReplyEventId.length > 0
    readonly property bool matrixEditMode: matrixEditEventId.length > 0
    property color threadColor: roomModel ? TimelineManager.userColor(roomModel.thread, palette.base) : palette.buttonText
    readonly property string matrixReplyPreviewUserId: matrixReplySenderId !== ""
        ? matrixReplySenderId
        : (matrixReplyDisplayName !== "" ? matrixReplyDisplayName : matrixReplyEventId)
    readonly property color matrixReplyPreviewWindowColor: (Komai.colors && Komai.colors.window !== undefined)
        ? Komai.colors.window
        : replyPopup.palette.window
    readonly property color matrixReplyPreviewBaseColor: (Komai.colors && Komai.colors.base !== undefined)
        ? Komai.colors.base
        : replyPopup.palette.base
    readonly property bool layoutVisible: matrixReplyMode
        || matrixEditMode
        || !!(roomModel && (roomModel.reply || roomModel.thread || roomModel.edit))
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
            visible: !!(roomModel && roomModel.thread) && !replyPopup.matrixReplyMode
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

                toolTipText: qsTr("Close")
                toolTipVisible: hovered
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredHeight: replyPopup.headerIconSize
                Layout.preferredWidth: replyPopup.headerIconSize
                hoverEnabled: true
                image: ":/icons/icons/ui/dismiss.svg"

                onClicked: roomModel.thread = undefined
            }
        }

        // ── Reply header (visible when replying to a specific message) ──
        RowLayout {
            visible: replyPopup.matrixReplyMode || !!(roomModel && roomModel.reply)
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
                text: replyPopup.matrixReplyMode
                    ? (replyPopup.matrixReplyDisplayName !== ""
                        ? qsTr("Replying to %1").arg(replyPopup.matrixReplyDisplayName)
                        : qsTr("Replying to this message"))
                    : (replyPreview.replyDisplayName !== ""
                        ? qsTr("Replying to %1").arg(replyPreview.replyDisplayName)
                    : qsTr("Replying to this message")
                    )
            }

            Item {
                Layout.fillWidth: true
            }

            ImageButton {
                id: closeReplyButton

                toolTipText: qsTr("Close")
                toolTipVisible: hovered
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredHeight: replyPopup.headerIconSize
                Layout.preferredWidth: replyPopup.headerIconSize
                hoverEnabled: true
                image: ":/icons/icons/ui/dismiss.svg"

                onClicked: {
                    if (replyPopup.matrixReplyMode)
                        TimelineManager.clearActiveMatrixReply();
                    else
                        roomModel.reply = undefined;
                }
            }
        }

        // ── Edit header (visible when editing a message) ──
        RowLayout {
            visible: replyPopup.matrixEditMode
                || (!!(roomModel && roomModel.edit) && !replyPopup.matrixReplyMode)
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

                toolTipText: qsTr("Close")
                toolTipVisible: hovered
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredHeight: replyPopup.headerIconSize
                Layout.preferredWidth: replyPopup.headerIconSize
                hoverEnabled: true
                image: ":/icons/icons/ui/dismiss.svg"

                onClicked: {
                    if (replyPopup.matrixEditMode)
                        TimelineManager.clearActiveMatrixEdit();
                    else
                        roomModel.edit = undefined;
                }
            }
        }

        // ── Reply preview (visible when replying to a specific message) ──
        Reply {
            id: replyPreview

            visible: !!(roomModel && roomModel.reply) && !replyPopup.matrixReplyMode

            property var modelData: (roomModel
                    && typeof roomModel.getDump === "function"
                    && roomModel.reply)
                ? roomModel.getDump(roomModel.reply, roomModel.id)
                : {}
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
            eventId: roomModel?.reply ?? ""
            bubblePalette: roomModel ? TimelineManager.roomUserBubblePalette(roomModel.roomId, replyUserId, roomColor, Settings.timelineUserColorCodingPolicy) : TimelineManager.userBubblePalette(replyUserId, roomColor)
            userColor: isReplyFromCurrentUser
                ? Komai.theme.userColorSelf
                : roomModel ? TimelineManager.roomUserColor(roomModel.roomId, replyUserId, previewWindowColor, Settings.timelineUserColorCodingPolicy) : TimelineManager.userColor(replyUserId, previewWindowColor)
            roomColor: isReplyFromCurrentUser
                ? Komai.theme.userColorSelf
                : roomModel ? TimelineManager.roomUserColor(roomModel.roomId, replyUserId, previewBaseColor, Settings.timelineUserColorCodingPolicy) : TimelineManager.userColor(replyUserId, previewBaseColor)
            maxWidth: parent.width
            limitHeight: true
        }

        Reply {
            id: matrixReplyPreview

            visible: replyPopup.matrixReplyMode
            enabled: false
            width: parent.width
            eventId: replyPopup.matrixReplyEventId
            previewData: ({
                    "userId": replyPopup.matrixReplyPreviewUserId,
                    "userName": replyPopup.matrixReplyDisplayName !== ""
                        ? replyPopup.matrixReplyDisplayName
                        : qsTr("Reply"),
                    "body": replyPopup.matrixReplyBody,
                    "formattedBody": TimelineManager.formatMatrixMessageHtml(replyPopup.matrixReplyBody),
                    "isOnlyEmoji": 0
                })
            roomModelOverride: replyPopup.roomModel
            bubblePalette: replyPopup.roomModel
                ? TimelineManager.roomUserBubblePalette(replyPopup.roomModel.roomId,
                                                        replyPopup.matrixReplyPreviewUserId,
                                                        roomColor,
                                                        Settings.timelineUserColorCodingPolicy)
                : TimelineManager.userBubblePalette(replyPopup.matrixReplyPreviewUserId, roomColor)
            userColor: replyPopup.roomModel
                ? TimelineManager.roomUserColor(replyPopup.roomModel.roomId,
                                                replyPopup.matrixReplyPreviewUserId,
                                                replyPopup.matrixReplyPreviewWindowColor,
                                                Settings.timelineUserColorCodingPolicy)
                : TimelineManager.userColor(replyPopup.matrixReplyPreviewUserId,
                                            replyPopup.matrixReplyPreviewWindowColor)
            roomColor: replyPopup.roomModel
                ? TimelineManager.roomUserColor(replyPopup.roomModel.roomId,
                                                replyPopup.matrixReplyPreviewUserId,
                                                replyPopup.matrixReplyPreviewBaseColor,
                                                Settings.timelineUserColorCodingPolicy)
                : TimelineManager.userColor(replyPopup.matrixReplyPreviewUserId,
                                            replyPopup.matrixReplyPreviewBaseColor)
            maxWidth: parent.width
            limitHeight: true
        }
    }

}
