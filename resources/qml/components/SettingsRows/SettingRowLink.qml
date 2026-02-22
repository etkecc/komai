// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import QtQuick

Text {
    id: root

    required property var model

    color: palette.text
    text: model.value
    textFormat: Text.RichText
    onLinkActivated: function(link) {
        Qt.openUrlExternally(link);
    }

    MouseArea {
        anchors.fill: parent
        cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
        acceptedButtons: Qt.NoButton
    }
}
