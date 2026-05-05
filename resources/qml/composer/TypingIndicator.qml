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
    readonly property bool hasTypingUsers: room && room.typingUsers.length > 0
    readonly property int iconSide: fontMetrics.ascent

    Layout.fillWidth: true
    implicitHeight: Math.max(fontMetrics.height * 1.2, typingDisplay.implicitHeight)

    Row {
        id: typingDisplay

        anchors.verticalCenter: parent.verticalCenter
        anchors.left: alignRightByPositioning ? undefined : parent.left
        anchors.leftMargin: alignRightByPositioning ? 0 : 10
        anchors.right: alignRightByPositioning ? parent.right : undefined
        anchors.rightMargin: alignRightByPositioning ? 10 : 0
        spacing: Komai.paddingSmall
        // Row is a positioner, so it inherits LayoutMirroring from the parent —
        // children flow right-to-left in RTL locales without an explicit
        // layoutDirection here.
        visible: hasTypingUsers

        Image {
            anchors.verticalCenter: typingLabel.verticalCenter
            // image://colorimage tints the SVG to the requested color, so the
            // glyph picks up palette.text the same way the label does.
            source: hasTypingUsers
                ? "image://colorimage/:/icons/icons/ui/keyboard-shortcut.svg?" + palette.text
                : ""
            width: iconSide
            height: iconSide
            sourceSize.width: iconSide
            sourceSize.height: iconSide
            fillMode: Image.PreserveAspectFit
            mipmap: true
        }

        Label {
            id: typingLabel

            color: palette.text
            text: room ? room.formatTypingUsers(room.typingUsers, palette.base) : ""
            textFormat: Text.RichText
        }
    }
}
