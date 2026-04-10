// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import cc.etke.komai

Rectangle {
    id: root

    readonly property string komaiProjectLink: "<a href=\"https://github.com/etkecc/komai\">Komai</a>"
    readonly property string etkeProjectLink: "<a href=\"https://etke.cc/?utm_source=komai&utm_medium=app&utm_campaign=attribution\">etke.cc</a>"

    implicitHeight: Komai.navigationRowHeight
    Layout.fillWidth: true
    Layout.preferredHeight: implicitHeight
    color: palette.alternateBase

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 1
        color: Komai.theme.separator
    }

    Text {
        anchors.centerIn: parent
        width: Math.min(parent.width - Komai.paddingMedium * 2, 760)
        textFormat: Text.RichText
        font.pointSize: Settings.uiFontSizePt
        color: palette.buttonText
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        text: "<style>a { color: " + palette.highlight + "; }</style>" +
              qsTr("%1 is created by %2 (managed Matrix server hosting).")
              .arg(root.komaiProjectLink)
              .arg(root.etkeProjectLink)

        onLinkActivated: function(link) { Qt.openUrlExternally(link) }

        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.NoButton
            cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
        }
    }
}
