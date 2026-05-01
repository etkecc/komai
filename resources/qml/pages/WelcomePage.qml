// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import cc.etke.komai 1.0
import "onboarding" as Onboarding
import "welcome" as Welcome

Rectangle {
    id: root
    // Wide enough for the 1.5×-font tagline to fit on one line in most
    // languages. Some translations (e.g. Bulgarian, Brazilian Portuguese)
    // are still long enough to wrap, but the wrap is much shallower than at
    // the previous 760 limit.
    property int maxExpansion: 880

    readonly property string matrixUrl: "https://matrix.org/"
    readonly property string komaiMeaningUrl: "https://en.wiktionary.org/wiki/%E3%81%93%E3%81%BE%E3%81%84"
    readonly property string secretsStorageDocsUrl: "https://github.com/etkecc/komai/blob/main/docs/user-guide/settings/secret-storage.md"

    color: palette.window

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Onboarding.OnboardingScrollPage {
            id: scroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            maxContentWidth: root.maxExpansion
            topSpacerHeight: Komai.paddingLarge * 2

            Image {
                Layout.alignment: Qt.AlignHCenter
                source: "qrc:/logos/komai.svg"
                sourceSize.width: 256
                sourceSize.height: 256
                Layout.preferredHeight: 256
                Layout.preferredWidth: 256
                fillMode: Image.PreserveAspectFit
                Accessible.ignored: true
            }

            Welcome.WelcomeRichText {
                readonly property string plainText: qsTr("Welcome to Komai")
                Layout.topMargin: Komai.paddingLarge
                Layout.leftMargin: Komai.paddingLarge
                Layout.rightMargin: Komai.paddingLarge
                Layout.alignment: Qt.AlignHCenter
                Layout.fillWidth: true
                text: "<style>a { color: " + palette.highlight + "; text-decoration: none; }</style>" +
                      plainText +
                      " (<a href=\"" + root.komaiMeaningUrl + "\">こまい</a>)"
                color: palette.text
                font.pointSize: Settings.uiFontSizePt * 2
                horizontalAlignment: Text.AlignHCenter
                Accessible.role: Accessible.Heading
                Accessible.name: plainText
            }

            Welcome.WelcomeRichText {
                readonly property string plainText: Komai.taglineTemplate.arg(Komai.matrixWord)
                Layout.topMargin: Komai.paddingSmall
                Layout.leftMargin: Komai.paddingLarge
                Layout.rightMargin: Komai.paddingLarge
                Layout.bottomMargin: Komai.paddingLarge
                Layout.alignment: Qt.AlignHCenter
                Layout.fillWidth: true
                text: "<style>a { color: " + palette.highlight + "; }</style>" +
                      Komai.taglineTemplate.arg("<a href=\"" + root.matrixUrl + "\">" + Komai.matrixWord + "</a>")
                color: palette.buttonText
                font.pointSize: Settings.uiFontSizePt * 1.5
                horizontalAlignment: Text.AlignHCenter
                Accessible.role: Accessible.Heading
                Accessible.name: plainText
            }

            Welcome.WelcomeRichText {
                readonly property string plainText: qsTr("Secure secret storage (OS keychain) is not available in this environment, so Komai is using file-based secret storage for now. This is less secure.")
                visible: Settings.secretsProviderFallbackWarningVisible
                Layout.topMargin: Komai.paddingSmall
                Layout.leftMargin: Komai.paddingLarge
                Layout.rightMargin: Komai.paddingLarge
                Layout.bottomMargin: Komai.paddingLarge
                Layout.alignment: Qt.AlignHCenter
                Layout.fillWidth: true
                text: "<style>a { color: " + palette.highlight + "; }</style>" +
                      plainText +
                      " " +
                      "<a href=\"" + root.secretsStorageDocsUrl + "\">" + qsTr("Learn more") + "</a>"
                color: Komai.theme.attention
                font.pointSize: Settings.uiFontSizePt * 1.05
                horizontalAlignment: Text.AlignHCenter
                Accessible.role: Accessible.AlertMessage
                Accessible.name: plainText
            }

            Welcome.WelcomePrimaryActions {
                onNewToMatrixRequested: {
                    mainWindow.push(newToMatrixPage);
                }
                onRegisterRequested: {
                    mainWindow.push(registerPage);
                }
                onLoginRequested: {
                    mainWindow.push(loginPage);
                }
            }

            Label {
                Layout.topMargin: Komai.paddingLarge
                Layout.bottomMargin: Komai.paddingSmall
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("An early touch of personality")
                color: palette.buttonText
                font.pointSize: Settings.uiFontSizePt * 1.1
                Accessible.role: Accessible.Heading
            }

            Welcome.WelcomePersonalityControls {
                Layout.alignment: Qt.AlignHCenter
                Layout.leftMargin: Komai.paddingLarge
                Layout.rightMargin: Komai.paddingLarge
            }

        }

        AttributionFooter { showSponsor: false }
    }
}
