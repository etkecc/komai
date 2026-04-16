// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai

Rectangle {
    id: root

    property bool showSponsor: true
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

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Komai.paddingMedium
        anchors.rightMargin: Komai.paddingMedium
        spacing: Komai.paddingMedium

        Image {
            id: footerLogo

            readonly property real logoSize: footerText.implicitHeight * 1.5

            Layout.preferredWidth: logoSize
            Layout.preferredHeight: logoSize
            Layout.alignment: Qt.AlignVCenter
            source: "qrc:/logos/komai.svg"
            sourceSize: Qt.size(logoSize * 2, logoSize * 2)

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: Qt.openUrlExternally("https://github.com/etkecc/komai")
            }
        }

        Text {
            id: footerText

            Layout.fillWidth: true
            textFormat: Text.RichText
            font.pointSize: Settings.uiFontSizePt
            color: palette.buttonText
            horizontalAlignment: Text.AlignLeft
            elide: Text.ElideRight
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

        KomaiButton {
            id: supportButton

            readonly property string heartIcon: Settings.sponsoringStatus === "sponsoring"
                ? "qrc:/icons/icons/ui/heart-filled.svg" : "qrc:/icons/icons/ui/heart.svg"

            visible: root.showSponsor && Settings.sponsoringStatus !== "hidden"
            text: Settings.sponsoringStatus === "sponsoring" ? qsTr("Sponsoring!") : qsTr("Sponsor")
            icon.source: "image://colorimage/:" + heartIcon.substring(4) + "?" + Komai.theme.error
            onClicked: supportDialog.open()
        }

        KomaiButton {
            text: qsTr("Report an issue")
            icon.source: "qrc:/icons/icons/ui/bug.svg"
            onClicked: Qt.openUrlExternally("https://github.com/etkecc/komai/issues")
        }
    }

    SupportDialog {
        id: supportDialog
    }
}
