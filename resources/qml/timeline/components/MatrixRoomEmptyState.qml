// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import cc.etke.komai

ColumnLayout {
    id: root

    required property var rootItem

    anchors.centerIn: parent
    spacing: Komai.paddingMedium
    visible: !root.rootItem.hasTimeline
    width: Math.min(parent.width - Komai.paddingLarge * 2, 560)

    MatrixText {
        Layout.fillWidth: true
        horizontalAlignment: TextEdit.AlignHCenter
        text: root.rootItem.loading
            ? qsTr("Loading this room…")
            : qsTr("Nothing has loaded for this room yet.")
        wrapMode: Text.WordWrap
    }
}
