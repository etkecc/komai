// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import ".."
import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import im.nheko 1.0

ItemDelegate {
    property alias bgColor: background.color
    property alias userid: avatar.userid
    property alias displayName: avatar.displayName
    property string avatarUrl
    property string roomId: ""
    implicitHeight: layout.implicitHeight + Nheko.paddingSmall * 2
    background: Rectangle {id: background}
    GridLayout {
        id: layout
        anchors.centerIn: parent
        width: parent.width - Nheko.paddingSmall * 2
        rows: 2
        columns: 2
        rowSpacing: Nheko.paddingSmall
        columnSpacing: Nheko.paddingMedium

        Avatar {
            id: avatar
            Layout.rowSpan: 2
            Layout.preferredWidth: Nheko.avatarSize
            Layout.preferredHeight: Nheko.avatarSize
            Layout.alignment: Qt.AlignLeft
            url: avatarUrl.replace("mxc://", "image://MxcImage/")
            enabled: false
        }
        Label {
            Layout.fillWidth: true
            text: displayName
            color: Qt.darker(roomId ? TimelineManager.roomUserColor(roomId, userid, palette.window, palette.highlight) : TimelineManager.userColor(userid, palette.window), 1.3)
            font.pointSize: Settings.fontSize
        }

        Label {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            text: userid
            color: palette.buttonText
            font.pointSize: Settings.fontSize * 0.9
        }
    }
}
