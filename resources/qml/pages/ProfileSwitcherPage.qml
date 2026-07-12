// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../components"
import QtQuick
import QtQuick.Layouts
import cc.etke.komai
import "welcome" as Welcome

Item {
    id: root
    readonly property string komaiProjectUrl: "https://komai.chat/?utm_source=komai&amp;utm_medium=app&amp;utm_campaign=profile-switcher"
    readonly property string matrixUrl: "https://matrix.org/"

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Komai.paddingLarge
        spacing: Komai.paddingMedium

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: Komai.paddingMedium
        }

        Image {
            Layout.alignment: Qt.AlignHCenter
            source: "qrc:/logos/komai.svg"
            sourceSize.width: 160
            sourceSize.height: 160
            fillMode: Image.PreserveAspectFit
            Layout.preferredWidth: 160
            Layout.preferredHeight: 160
        }

        Welcome.WelcomeRichText {
            Layout.alignment: Qt.AlignHCenter
            Layout.fillWidth: true
            horizontalAlignment: Text.AlignHCenter
            text: "<style>a { color: " + palette.highlight + "; }</style>"
                + "<a href=\"" + root.komaiProjectUrl + "\">Komai</a><br/>"
                + Komai.taglineTemplate.arg("<a href=\"" + root.matrixUrl + "\">" + Komai.matrixWord + "</a>")
            color: palette.text
            font.bold: true
            font.pointSize: Settings.uiFontSizePt * 1.8
        }

        ApplicationProfilesView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            standalone: true
        }
    }
}
