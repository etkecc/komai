// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import im.nheko

SettingsContent {
    id: aboutTab
    tabFilter: UserSettingsModel.TabAbout
    readonly property string projectUrl: "https://github.com/etkecc/komai"
    readonly property string matrixUrl: "https://matrix.org/"
    readonly property string komaiMeaningUrl: "https://en.wiktionary.org/wiki/%E3%81%93%E3%81%BE%E3%81%84"

    headerContent: Component {
        ColumnLayout {
            Layout.fillWidth: true
            Layout.topMargin: Nheko.paddingLarge
            Layout.bottomMargin: Nheko.paddingMedium
            spacing: Nheko.paddingSmall

            Image {
                Layout.alignment: Qt.AlignHCenter
                source: "qrc:/logos/splash.png"
                Layout.preferredHeight: 128
                Layout.preferredWidth: 128
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                textFormat: Text.RichText
                text: "Komai (<a href=\"" + aboutTab.komaiMeaningUrl + "\">こまい</a>)"
                color: palette.text
                font.pointSize: Settings.uiFontSizePt * 2
                font.bold: true
                onLinkActivated: function(link) {
                    Qt.openUrlExternally(link);
                }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.NoButton
                    cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
                }
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                Layout.fillWidth: true
                textFormat: Text.RichText
                text: "<style>a { color: " + palette.highlight + "; }</style>" +
                      Nheko.taglineTemplate.arg("<a href=\"" + aboutTab.matrixUrl + "\">" + Nheko.matrixWord + "</a>")
                color: palette.buttonText
                font.pointSize: Settings.uiFontSizePt * 1.2
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
                onLinkActivated: function(link) {
                    Qt.openUrlExternally(link);
                }

                MouseArea {
                    anchors.fill: parent
                    acceptedButtons: Qt.NoButton
                    cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
                }
            }
        }
    }
}
