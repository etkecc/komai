// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components" as Components
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai

ColumnLayout {
    id: preview

    required property var room
    required property var roomPreview

    property string avatarUrl: room ? room.roomAvatarUrl : (roomPreview ? roomPreview.roomAvatarUrl : "")
    property string reason: roomPreview ? roomPreview.reason : ""
    property string roomId: room ? room.roomId : (roomPreview ? roomPreview.roomid : "")
    property string roomName: room ? room.roomName : (roomPreview ? roomPreview.roomName : "")
    property string roomTopic: room ? room.roomTopic : (roomPreview ? roomPreview.roomTopic : "")
    readonly property bool isMatrixSummaryPreview: !room && roomPreview && roomPreview.isMatrixSummary
    readonly property bool hasMatrixTimeline: isMatrixSummaryPreview && TimelineManager.matrixTimelineItemCount > 0
    readonly property bool matrixTimelineLoading: isMatrixSummaryPreview && TimelineManager.matrixTimelineLoading

    anchors.fill: parent
    anchors.margins: Komai.paddingLarge
    enabled: visible
    spacing: Komai.paddingLarge
    visible: room != null && room.isSpace || roomPreview != null

    Item {
        Layout.fillHeight: true
        Layout.fillWidth: true
        visible: !hasMatrixTimeline
    }
    Avatar {
        Layout.alignment: Qt.AlignHCenter
        displayName: parent.roomName
        enabled: false
        implicitHeight: Komai.listIconSize
        roomid: parent.roomId
        url: parent.avatarUrl.replace("mxc://", "image://MxcImage/")
        implicitWidth: Komai.listIconSize
    }

    MatrixText {
        horizontalAlignment: TextEdit.AlignHCenter
        Layout.fillWidth: true
        font.pixelSize: 24
        text: (!room && !(roomPreview?.isFetched ?? false)) ? qsTr("No preview available") : preview.roomName

    }
    ImageButton {
        Layout.alignment: Qt.AlignHCenter
        toolTipText: qsTr("Settings")
        toolTipVisible: hovered
        Layout.bottomMargin: Komai.paddingMedium

        hoverEnabled: true
        image: ":/icons/icons/ui/settings.svg"
        visible: !!room

        onClicked: TimelineManager.openRoomInfo(room.roomId, "settings")
    }

    RowLayout {
        Layout.alignment: Qt.AlignHCenter
        spacing: Komai.paddingMedium
        visible: !!room
        Layout.fillWidth: true

        MatrixText {
            Layout.preferredWidth: contentWidth
            text: qsTr("%n member(s)", "", room ? room.roomMemberCount : 0)
        }
        ImageButton {
            toolTipText: qsTr("View members of %1").arg(room ? room.roomName : "")
            toolTipVisible: hovered
            hoverEnabled: true
            image: ":/icons/icons/ui/people.svg"

            onClicked: TimelineManager.openRoomInfo(room.roomId, "members")
        }
    }
    ScrollView {
        id: topicScroll

        Layout.alignment: Qt.AlignHCenter
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        Layout.leftMargin: Komai.paddingLarge
        Layout.rightMargin: Komai.paddingLarge
        Layout.maximumHeight: preview.height / 3
        contentWidth: availableWidth

        MatrixText {
            width: topicScroll.availableWidth
            background: null
            horizontalAlignment: TextEdit.AlignHCenter
            text: (room || (roomPreview?.isFetched ?? false)) ? TimelineManager.escapeEmoji(preview.roomTopic) : qsTr("This room is possibly inaccessible. If this room is private, you should remove it from this community.")
            textFormat: TextEdit.RichText
        }
    }
    Rectangle {
        Layout.fillWidth: true
        Layout.fillHeight: true
        Layout.minimumHeight: 0
        color: "transparent"
        visible: hasMatrixTimeline

        ListView {
            id: matrixTimelineList

            anchors.fill: parent
            clip: true
            model: TimelineManager.matrixTimelineModel
            spacing: Komai.paddingMedium

            delegate: Item {
                required property string itemKind
                required property string senderDisplayName
                required property string body
                required property double timestamp
                required property bool isOwn

                width: ListView.view.width
                height: itemKind === "date_divider" ? dateDivider.implicitHeight : messageRow.implicitHeight

                Rectangle {
                    id: dateDivider

                    anchors.horizontalCenter: parent.horizontalCenter
                    color: palette.mid
                    height: dividerLabel.implicitHeight + Komai.paddingSmall * 2
                    radius: height / 2
                    visible: itemKind === "date_divider"
                    width: dividerLabel.implicitWidth + Komai.paddingLarge * 2

                    MatrixText {
                        id: dividerLabel

                        anchors.centerIn: parent
                        color: palette.base
                        text: Qt.formatDateTime(new Date(timestamp), "dddd, d MMMM")
                        textFormat: TextEdit.PlainText
                    }
                }

                Item {
                    id: messageRow

                    anchors.left: parent.left
                    anchors.right: parent.right
                    implicitHeight: bubbleColumn.implicitHeight
                    visible: itemKind !== "date_divider"

                    ColumnLayout {
                        id: bubbleColumn

                        anchors.left: isOwn ? undefined : parent.left
                        anchors.right: isOwn ? parent.right : undefined
                        spacing: Komai.paddingSmall
                        width: Math.min(parent.width * 0.82, Math.max(280, parent.width * 0.6))

                        MatrixText {
                            Layout.alignment: isOwn ? Qt.AlignRight : Qt.AlignLeft
                            color: palette.buttonText
                            text: senderDisplayName
                            textFormat: TextEdit.PlainText
                        }

                        Rectangle {
                            Layout.alignment: isOwn ? Qt.AlignRight : Qt.AlignLeft
                            color: isOwn ? palette.highlight : palette.alternateBase
                            implicitHeight: bubbleBody.implicitHeight + Komai.paddingMedium * 2
                            implicitWidth: Math.min(parent.width, bubbleBody.implicitWidth + Komai.paddingLarge * 2)
                            radius: Komai.paddingMedium * 2

                            MatrixText {
                                id: bubbleBody

                                anchors.fill: parent
                                anchors.margins: Komai.paddingMedium
                                color: isOwn ? palette.highlightedText : palette.text
                                text: body
                                textFormat: TextEdit.PlainText
                                width: parent.width - Komai.paddingMedium * 2
                                wrapMode: Text.WordWrap
                            }
                        }
                    }
                }
            }
        }
    }
    Components.KomaiButton {
        Layout.alignment: Qt.AlignHCenter
        text: qsTr("Join the conversation")
        highlighted: true
        visible: roomPreview && roomPreview.canJoin

        onClicked: Rooms.joinPreview(roomPreview.roomid)
    }
    Components.KomaiButton {
        Layout.alignment: Qt.AlignHCenter
        text: qsTr("Accept invite")
        highlighted: true
        visible: roomPreview && roomPreview.isInvite

        onClicked: Rooms.acceptInvite(roomPreview.roomid)
    }
    Components.KomaiButton {
        Layout.alignment: Qt.AlignHCenter
        text: qsTr("Decline invite")
        visible: roomPreview && roomPreview.isInvite

        onClicked: Rooms.declineInvite(roomPreview.roomid)
    }
    Components.KomaiButton {
        Layout.alignment: Qt.AlignHCenter
        text: qsTr("Decline invite and ignore user")
        visible: roomPreview && roomPreview.isInvite

        onClicked: {
            var inviter = TimelineManager.getGlobalUserProfile(roomPreview.inviterUserId)
            inviter.ignored = true
        }
    }
    Components.KomaiButton {
        Layout.alignment: Qt.AlignHCenter
        text: qsTr("Leave")
        visible: !!room

        onClicked: TimelineManager.openLeaveRoomDialog(room.roomId)
    }
    MatrixText {
        Layout.alignment: Qt.AlignHCenter
        Layout.maximumWidth: Math.max(320, preview.width * 0.6)
        horizontalAlignment: TextEdit.AlignHCenter
        text: matrixTimelineLoading
            ? qsTr("Loading room timeline…")
            : (isMatrixSummaryPreview
                ? qsTr("No timeline items are loaded for this room yet.")
                : qsTr("This room is available in the room list, but its timeline is not loaded yet."))
        visible: !room && roomPreview && !roomPreview.isInvite && !roomPreview.canJoin && !hasMatrixTimeline
        wrapMode: Text.WordWrap
    }
    RowLayout {
        Layout.alignment: Qt.AlignHCenter
        Layout.fillWidth: true
        spacing: Komai.paddingMedium
        visible: roomPreview && roomPreview.isInvite && reasonField.showReason

        MatrixText {
            Layout.maximumWidth: contentWidth
            Layout.preferredWidth: contentWidth
            Layout.fillWidth: true
            text: qsTr("Invited by %1 (%2)").arg(TimelineManager.escapeEmoji(inviterAvatar.displayName)).arg(TimelineManager.escapeEmoji(TimelineManager.htmlEscape(inviterAvatar.userid)))
        }
        Avatar {
            id: inviterAvatar

            Layout.alignment: Qt.AlignHCenter
            displayName: roomPreview?.inviterDisplayName ?? ""
            enabled: true
            implicitHeight: Komai.listIconSize
            roomid: preview.roomId
            url: (roomPreview?.inviterAvatarUrl ?? "").replace("mxc://", "image://MxcImage/")
            userid: roomPreview?.inviterUserId ?? ""
            implicitWidth: Komai.listIconSize

            onClicked: TimelineManager.openGlobalUserProfile(roomPreview.inviterUserId)
        }
    }
    ScrollView {
        id: reasonField

        property bool showReason: false

        Layout.alignment: Qt.AlignHCenter
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        visible: preview.reason !== "" && showReason
        contentWidth: availableWidth

        Components.KomaiTextArea {
            width: reasonField.availableWidth
            background: null
            horizontalAlignment: TextEdit.AlignHCenter
            readOnly: true
            text: TimelineManager.escapeEmoji(preview.reason)
            textFormat: TextEdit.RichText
            wrapMode: TextEdit.WordWrap
        }
    }
    Components.KomaiButton {
        id: showReasonButton

        Layout.alignment: Qt.AlignHCenter
        Layout.leftMargin: Komai.paddingLarge
        Layout.rightMargin: Komai.paddingLarge
        text: reasonField.showReason ? qsTr("Hide invite reason") : qsTr("Show invite reason")
        visible: roomPreview && roomPreview.isInvite

        onClicked: {
            reasonField.showReason = !reasonField.showReason;
        }
    }
    Item {
        Layout.preferredHeight: Math.ceil(fontMetrics.lineSpacing * 2)
        visible: room != null
    }
    Item {
        Layout.fillHeight: true
        visible: !hasMatrixTimeline
    }
}
