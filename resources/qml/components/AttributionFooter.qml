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
    readonly property string komaiProjectLink: "<a href=\"https://komai.chat/?utm_source=komai&utm_medium=app&utm_campaign=attribution\">Komai</a>"
    readonly property string etkeProjectLink: "<a href=\"https://etke.cc/?utm_source=komai&utm_medium=app&utm_campaign=attribution\">etke.cc</a>"
    // Single source of truth so the rendered HTML and the screen-reader name
    // can't drift apart.
    readonly property string attributionTemplate: qsTr("%1 is created by %2 (managed Matrix server hosting).")

    // Drop to a two-row layout (text above, buttons below) as soon as the full
    // attribution sentence would otherwise need to elide alongside the buttons.
    // Driving the threshold off footerText.implicitWidth keeps it adaptive to
    // locale and font size, where a hard-coded reservation underestimated
    // longer translations and let the buttons crowd into the elided text.
    readonly property real stackThreshold:
          footerLogo.logoSize
        + footerText.implicitWidth
        + buttonsGroup.implicitWidth
        + 4 * Komai.paddingMedium
    readonly property bool stacked: width > 0 && width < stackThreshold

    // Always reserve paddingSmall above and below the inner content. In single-row
    // mode the footer also keeps Komai.navigationRowHeight as a baseline so it
    // visually aligns with adjacent bars on Compact/Spacious — but it grows past
    // that baseline on Dense, where navigationRowHeight is shorter than the
    // buttons' implicit height and would otherwise crop them.
    implicitHeight: stacked
        ? (textGroup.implicitHeight + buttonsGroup.implicitHeight + content.rowSpacing + 2 * Komai.paddingSmall)
        : Math.max(Komai.navigationRowHeight,
                   Math.max(textGroup.implicitHeight, buttonsGroup.implicitHeight) + 2 * Komai.paddingSmall)
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

    GridLayout {
        id: content

        anchors.fill: parent
        anchors.leftMargin: Komai.paddingMedium
        anchors.rightMargin: Komai.paddingMedium
        anchors.topMargin: Komai.paddingSmall
        anchors.bottomMargin: Komai.paddingSmall
        columns: root.stacked ? 1 : 2
        rowSpacing: Komai.paddingSmall
        columnSpacing: Komai.paddingMedium

        RowLayout {
            id: textGroup

            Layout.fillWidth: !root.stacked
            Layout.alignment: root.stacked ? Qt.AlignHCenter : Qt.AlignVCenter
            spacing: Komai.paddingMedium

            Image {
                id: footerLogo

                readonly property real logoSize: footerText.implicitHeight * 1.5

                Layout.preferredWidth: logoSize
                Layout.preferredHeight: logoSize
                Layout.alignment: Qt.AlignVCenter
                source: "qrc:/logos/komai.svg"
                sourceSize: Qt.size(logoSize * 2, logoSize * 2)
                // The same Komai/etke.cc URLs are reachable via links in the
                // adjacent rich-text, so the logo is purely decorative for
                // assistive tech.
                Accessible.ignored: true

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: Qt.openUrlExternally("https://komai.chat/?utm_source=komai&utm_medium=app&utm_campaign=attribution")
                }
            }

            Text {
                id: footerText

                Layout.fillWidth: !root.stacked
                textFormat: Text.RichText
                font.pointSize: Settings.uiFontSizePt
                color: palette.buttonText
                horizontalAlignment: Text.AlignLeft
                elide: Text.ElideRight
                text: "<style>a { color: " + palette.link + "; }</style>" +
                      root.attributionTemplate.arg(root.komaiProjectLink).arg(root.etkeProjectLink)
                Accessible.name: root.attributionTemplate.arg("Komai").arg("etke.cc")

                onLinkActivated: function(link) { Qt.openUrlExternally(link) }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.NoButton
                    cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
                }
            }
        }

        RowLayout {
            id: buttonsGroup

            Layout.alignment: root.stacked ? Qt.AlignHCenter : Qt.AlignVCenter | Qt.AlignRight
            spacing: Komai.paddingMedium

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
    }

    SupportDialog {
        id: supportDialog
    }
}
