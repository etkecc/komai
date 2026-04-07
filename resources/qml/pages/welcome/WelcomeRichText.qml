// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick

Text {
    id: root

    textFormat: Text.RichText
    wrapMode: Text.Wrap

    onLinkActivated: function(link) {
        Qt.openUrlExternally(link)
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.NoButton
        cursorShape: root.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
    }
}
