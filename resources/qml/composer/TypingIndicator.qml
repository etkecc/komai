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
    readonly property bool mirrored: LayoutMirroring.enabled || Qt.application.layoutDirection === Qt.RightToLeft
    readonly property bool alignRightByPositioning: {
        switch (Settings.timelineMessagesLayoutPositioning) {
        case Settings.TimelineMessagesLayoutPositioning.AllRight:
            return true;
        case Settings.TimelineMessagesLayoutPositioning.AllLeft:
            return false;
        case Settings.TimelineMessagesLayoutPositioning.OpposingBySender:
        default:
            return mirrored;
        }
    }

    Layout.fillWidth: true
    implicitHeight: Math.max(fontMetrics.height * 1.2, typingDisplay.implicitHeight)

    Label {
        id: typingDisplay

        anchors.left: parent.left
        anchors.leftMargin: 10
        anchors.right: parent.right
        anchors.rightMargin: 10
        color: palette.text
        horizontalAlignment: alignRightByPositioning ? Text.AlignRight : Text.AlignLeft
        text: room ? room.formatTypingUsers(room.typingUsers, palette.base) : ""
        textFormat: Text.RichText
        visible: room && room.typingUsers.length > 0
    }
}
