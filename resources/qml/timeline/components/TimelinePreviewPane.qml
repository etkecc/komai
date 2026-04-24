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

    property var room: null
    property var roomPreview: null

    property string avatarUrl: room ? room.roomAvatarUrl : (roomPreview ? roomPreview.roomAvatarUrl : "")
    property string reason: roomPreview ? roomPreview.reason : ""
    property string roomId: room ? room.roomId : (roomPreview ? roomPreview.roomid : "")
    property string roomName: room ? room.roomName : (roomPreview ? roomPreview.roomName : "")
    property string roomTopic: room ? room.roomTopic : (roomPreview ? roomPreview.roomTopic : "")

    anchors.fill: parent
    anchors.margins: Komai.paddingLarge
    enabled: visible
    spacing: Komai.paddingLarge
    visible: room != null && room.isSpace || roomPreview != null

    Item {
        Layout.fillHeight: true
        Layout.fillWidth: true
    }
    Avatar {
        Layout.alignment: Qt.AlignHCenter
        displayName: parent.roomName
        enabled: false
        implicitHeight: Komai.iconSize
        roomid: parent.roomId
        url: parent.avatarUrl.replace("mxc://", "image://MxcImage/")
        implicitWidth: Komai.iconSize
    }

    MatrixText {
        horizontalAlignment: TextEdit.AlignHCenter
        Layout.fillWidth: true
        font.pointSize: Settings.uiFontSizePt * 2
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
    Components.KomaiButton {
        Layout.alignment: Qt.AlignHCenter
        text: qsTr("Join the conversation")
        highlighted: true
        visible: roomPreview && roomPreview.canJoin

        onClicked: Rooms.joinPreview(roomPreview.roomid)
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
        text: qsTr("This room is available in the room list, but its timeline is not loaded yet.")
        visible: !room && roomPreview && !roomPreview.isInvite && !roomPreview.canJoin
        wrapMode: Text.WordWrap
    }
    Item {
        Layout.preferredHeight: Math.ceil(fontMetrics.lineSpacing * 2)
        visible: room != null
    }
    Item {
        Layout.fillHeight: true
        visible: true
    }
}
