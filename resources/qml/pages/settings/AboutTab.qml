// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import cc.etke.komai

SettingsContent {
    id: aboutTab
    tabFilter: UserSettingsModel.TabAbout
    readonly property string matrixUrl: "https://matrix.org/"
    readonly property string komaiMeaningUrl: "https://en.wiktionary.org/wiki/%E3%81%93%E3%81%BE%E3%81%84"

    headerContent: Component {
        ColumnLayout {
            Layout.fillWidth: true
            Layout.topMargin: Komai.paddingLarge
            Layout.bottomMargin: Komai.paddingMedium
            spacing: Komai.paddingSmall

            Image {
                Layout.alignment: Qt.AlignHCenter
                source: "qrc:/logos/komai.svg"
                sourceSize.width: 128
                sourceSize.height: 128
                Layout.preferredHeight: 128
                Layout.preferredWidth: 128
                fillMode: Image.PreserveAspectFit
            }

            Text {
                Layout.alignment: Qt.AlignHCenter
                textFormat: Text.RichText
                text: "<style>a { color: " + palette.highlight + "; text-decoration: none; }</style>" +
                      "Komai (<a href=\"" + aboutTab.komaiMeaningUrl + "\">こまい</a>)"
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
                      Komai.taglineTemplate.arg("<a href=\"" + aboutTab.matrixUrl + "\">" + Komai.matrixWord + "</a>")
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
