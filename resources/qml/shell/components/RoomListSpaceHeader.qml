// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai

Rectangle {
    id: root

    required property bool collapsed
    required property int avatarSize

    readonly property bool active: Communities.currentTagId.startsWith("space:")
    readonly property string spaceId: active ? Communities.currentTagId.substring(6) : ""
    readonly property var spaceRoom: active ? Rooms.getRoomById(spaceId) : null

    visible: active
    height: active ? Komai.navigationRowHeight : 0
    color: palette.alternateBase

    RowLayout {
        anchors.fill: parent
        anchors.margins: Komai.paddingMedium
        spacing: Komai.paddingMedium

        Avatar {
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: root.avatarSize
            Layout.preferredHeight: root.avatarSize
            displayName: root.spaceRoom ? root.spaceRoom.roomName : ""
            roomid: root.spaceId
            url: root.spaceRoom ? root.spaceRoom.roomAvatarUrl.replace("mxc://", "image://MxcImage/") : ""
            enabled: false
        }

        Label {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignVCenter
            text: root.spaceRoom ? root.spaceRoom.roomName : ""
            font.bold: true
            font.pixelSize: Komai.fontPixelSize
            elide: Text.ElideRight
            color: palette.text
            visible: !root.collapsed
        }
    }

    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        color: Komai.theme.separator
        height: 1
    }
}
