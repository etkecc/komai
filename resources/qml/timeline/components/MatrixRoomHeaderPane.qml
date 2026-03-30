// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../room/components"
import QtQuick
import QtQuick.Layouts

ColumnLayout {
    id: root

    required property var roomPreview
    required property bool showBackButton
    required property bool perfDisableRoomHeader
    required property var headerRoomModel

    property alias headerItem: roomHeader
    property alias searchHasFocus: roomHeader.searchHasFocus

    Layout.fillWidth: true
    spacing: 0

    RoomHeader {
        id: roomHeader

        Layout.fillWidth: true
        Layout.minimumHeight: visible ? implicitHeight : 0
        Layout.preferredHeight: visible ? implicitHeight : 0
        Layout.maximumHeight: visible ? implicitHeight : 0
        room: null
        roomModel: root.headerRoomModel
        roomId: root.roomPreview ? root.roomPreview.roomid : ""
        roomName: root.roomPreview ? root.roomPreview.roomName : qsTr("No room selected")
        avatarDisplayName: root.roomPreview ? root.roomPreview.roomName : qsTr("No room selected")
        avatarUrl: root.roomPreview ? root.roomPreview.roomAvatarUrl : ""
        directChatOtherUserId: root.roomPreview ? root.roomPreview.directChatOtherUserId : ""
        isDirect: !!root.roomPreview && root.roomPreview.isDirect
        isEncrypted: !!root.roomPreview && root.roomPreview.isEncrypted
        roomTopic: root.roomPreview ? root.roomPreview.roomTopic : ""
        showBackButton: root.showBackButton
        visible: !root.perfDisableRoomHeader
    }

    Rectangle {
        Layout.fillWidth: true
        Layout.minimumHeight: visible ? 1 : 0
        Layout.preferredHeight: visible ? 1 : 0
        Layout.maximumHeight: visible ? 1 : 0
        color: palette.mid
        visible: !root.perfDisableRoomHeader
    }
}
