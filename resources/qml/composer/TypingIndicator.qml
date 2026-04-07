// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai 1.0

Item {
    property var room: null
    readonly property bool alignRightByPositioning:
        Settings.timelineMessagesPositioning === Settings.TimelineMessagesPositioning.AllRight

    Layout.fillWidth: true
    implicitHeight: Math.max(fontMetrics.height * 1.2, typingDisplay.height)

    Rectangle {
        id: typingRect

        anchors.fill: parent
        color: palette.base
        visible: (room && room.typingUsers.length > 0)
        z: 3

        Label {
            id: typingDisplay

            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.leftMargin: 10
            anchors.right: parent.right
            anchors.rightMargin: 10
            color: palette.text
            horizontalAlignment: alignRightByPositioning ? Text.AlignRight : Text.AlignLeft
            text: room ? room.formatTypingUsers(room.typingUsers, palette.base) : ""
            textFormat: Text.RichText
        }
    }
}
