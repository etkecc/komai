// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.12
import QtQuick.Controls 2.12
import QtQuick.Layouts 1.12
import cc.etke.komai 1.0

ItemDelegate {
    property alias bgColor: background.color
    property alias userid: avatar.userid
    property alias displayName: avatar.displayName
    property string avatarUrl
    property string roomId: ""
    implicitHeight: layout.implicitHeight + Komai.paddingSmall * 2
    background: Rectangle {id: background}
    GridLayout {
        id: layout
        anchors.centerIn: parent
        width: parent.width - Komai.paddingSmall * 2
        rows: 2
        columns: 2
        rowSpacing: Komai.paddingSmall
        columnSpacing: Komai.paddingMedium

        Avatar {
            id: avatar
            Layout.rowSpan: 2
            Layout.preferredWidth: Komai.listIconSize
            Layout.preferredHeight: Komai.listIconSize
            Layout.alignment: Qt.AlignLeft
            url: avatarUrl.replace("mxc://", "image://MxcImage/")
            enabled: false
        }
        Label {
            Layout.fillWidth: true
            text: displayName
            color: Qt.darker(roomId ? TimelineManager.roomUserColor(roomId, userid, palette.window, Settings.timelineUserColorCodingPolicy) : TimelineManager.userColor(userid, palette.window), 1.3)
            font.pointSize: Settings.uiFontSizePt
        }

        Label {
            Layout.fillWidth: true
            Layout.alignment: Qt.AlignTop
            text: userid
            color: palette.buttonText
            font.pointSize: Settings.uiFontSizePt * 0.9
        }
    }
}
