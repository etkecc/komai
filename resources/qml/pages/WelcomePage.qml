// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.2
import QtQuick.Window 2.15
import cc.etke.komai 1.0
import "onboarding" as Onboarding
import "welcome" as Welcome

Item {
    id: root
    property int maxExpansion: 760

    readonly property string matrixUrl: "https://matrix.org/"
    readonly property string komaiMeaningUrl: "https://en.wiktionary.org/wiki/%E3%81%93%E3%81%BE%E3%81%84"
    readonly property string secretsStorageDocsUrl: "https://github.com/etkecc/komai/blob/main/docs/user-guide/settings/secret-storage.md"
    readonly property string komaiProjectLink: "<a href=\"https://github.com/etkecc/komai\">Komai</a>"
    readonly property string nhekoProjectLink: "<a href=\"https://nheko.im\">nheko</a>"
    readonly property string etkeProjectLink: "<a href=\"https://etke.cc/\">etke.cc</a>"

    Onboarding.OnboardingScrollPage {
        id: scroll
        anchors.fill: parent
        maxContentWidth: root.maxExpansion
        topSpacerHeight: Komai.paddingLarge * 2

            Image {
                Layout.alignment: Qt.AlignHCenter
                source: "qrc:/logos/splash.png"
                Layout.preferredHeight: 256
                Layout.preferredWidth: 256
            }

            Welcome.WelcomeRichText {
                Layout.topMargin: Komai.paddingLarge
                Layout.leftMargin: Komai.paddingLarge
                Layout.rightMargin: Komai.paddingLarge
                Layout.alignment: Qt.AlignHCenter
                Layout.fillWidth: true
                text: "<style>a { color: " + palette.highlight + "; text-decoration: none; }</style>" +
                      qsTr("Welcome to Komai") +
                      " (<a href=\"" + root.komaiMeaningUrl + "\">こまい</a>)"
                color: palette.text
                font.pointSize: Settings.uiFontSizePt * 2
                horizontalAlignment: Text.AlignHCenter
            }

            Welcome.WelcomeRichText {
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
            }

            Welcome.WelcomeRichText {
                visible: Settings.secretsProviderFallbackWarningVisible
                Layout.topMargin: Komai.paddingSmall
                Layout.leftMargin: Komai.paddingLarge
                Layout.rightMargin: Komai.paddingLarge
                Layout.bottomMargin: Komai.paddingLarge
                Layout.alignment: Qt.AlignHCenter
                Layout.fillWidth: true
                text: "<style>a { color: " + palette.highlight + "; }</style>" +
                      qsTr("Secure secret storage (OS keychain) is not available in this environment, so Komai is using file-based secret storage for now. This is less secure.") +
                      " " +
                      "<a href=\"" + root.secretsStorageDocsUrl + "\">" + qsTr("Learn more") + "</a>"
                color: Komai.theme.red
                font.pointSize: Settings.uiFontSizePt * 1.05
                horizontalAlignment: Text.AlignHCenter
            }

            Welcome.WelcomePrimaryActions {
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
            }

            Welcome.WelcomeThemeControls {
                Layout.alignment: Qt.AlignHCenter
                Layout.leftMargin: Komai.paddingLarge
                Layout.rightMargin: Komai.paddingLarge
            }

            Welcome.WelcomeRichText {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: Komai.paddingLarge
                Layout.bottomMargin: Komai.paddingLarge
                Layout.leftMargin: Komai.paddingLarge
                Layout.rightMargin: Komai.paddingLarge
                Layout.fillWidth: true
                font.pointSize: Settings.uiFontSizePt * 0.9
                text: "<style>a { color: " + palette.highlight + "; }</style>" +
                      qsTr("%1 is an opinionated UI/UX polished fork of %2, maintained by %3.")
                      .arg(root.komaiProjectLink)
                      .arg(root.nhekoProjectLink)
                      .arg(root.etkeProjectLink)
                color: palette.buttonText
                horizontalAlignment: Text.AlignHCenter
            }
    }
}
