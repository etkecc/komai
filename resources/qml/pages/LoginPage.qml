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
import "../components/"
import "../ui/"

Item {
    id: loginPage
    property int maxExpansion: 400

    property string error: login.error
    property bool hasPendingLoginInput: matrixIdLabel.text !== login.mxid || (login.homeserverNeeded && hsLabel.text !== login.homeserver)
    property bool loginEnabled: login.homeserverValid && !hasPendingLoginInput

    Login {
        id: login
    }

    Onboarding.OnboardingScrollPage {
        id: scroll
        anchors.fill: parent
        maxContentWidth: loginPage.maxExpansion
        topSpacerHeight: Komai.paddingLarge * 3

            Image {
                Layout.alignment: Qt.AlignHCenter
                source: "qrc:/logos/login.png"
                Layout.preferredHeight: 128
                Layout.preferredWidth: 128
            }

            Label {
                Layout.topMargin: Komai.paddingMedium
                Layout.leftMargin: Komai.paddingLarge
                Layout.rightMargin: Komai.paddingLarge
                Layout.bottomMargin: Komai.paddingLarge
                Layout.alignment: Qt.AlignHCenter
                Layout.fillWidth: true
                text: qsTr("Login to your Matrix account")
                color: palette.text
                font.pointSize: Settings.uiFontSizePt * 1.5
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
            }

            RowLayout {
                spacing: Komai.paddingLarge

                Layout.fillWidth: true
                MatrixTextField {
                    id: matrixIdLabel
                    label: qsTr("Matrix ID")
                    placeholderText: qsTr("e.g @user:yourserver.example.com")
                    onEditingFinished: login.mxid = text

                    toolTipText: qsTr("Your login name. A mxid should start with @ followed by the user ID. After the user ID you need to include your server name after a :.\nYou can also put your homeserver address there if your server doesn't support .well-known lookup.\nExample: @user:yourserver.example.com\nIf Komai fails to discover your homeserver, it will show you a field to enter the server manually.")
                    Keys.forwardTo: [pwBtn, ssoRepeater]
                }


                Spinner {
                    Layout.preferredHeight: matrixIdLabel.height/2
                    Layout.alignment: Qt.AlignBottom

                    visible: running
                    running: login.lookingUpHs
                    foreground: palette.mid
                }
            }

            MatrixText {
                Layout.fillWidth: true
                textFormat: Text.PlainText
                color: Komai.theme.error
                text: login.mxidError
                visible: text
                wrapMode: TextEdit.Wrap
            }
            RowLayout {

                MatrixTextField {
                    id: passwordLabel
                    Layout.fillWidth: true
                    label: qsTr("Password")
                    echoMode: TextInput.Password
                    toolTipText: qsTr("Your password.")
                    visible: login.passwordSupported
                    Keys.forwardTo: [pwBtn, ssoRepeater]
                }

                ImageButton {
                    id: showPwButton
                    Layout.preferredWidth: 30
                    Layout.preferredHeight: 30
                    visible: login.passwordSupported
                    Layout.alignment: Qt.AlignBottom
                    image: passwordLabel.echoMode === TextInput.Password ? ":/icons/icons/ui/eye-show.svg" : ":/icons/icons/ui/eye-hide.svg"
                    toolTipVisible: hovered
                    toolTipText: qsTr("Show/Hide Password")
                    onClicked: {
                        if (passwordLabel.echoMode === TextInput.Normal) {
                            passwordLabel.echoMode = TextInput.Password
                        }
                        else {
                            passwordLabel.echoMode = TextInput.Normal
                        }
                    }
                }
            }

            MatrixTextField {
                id: deviceNameLabel
                Layout.fillWidth: true
                label: qsTr("Device name")
                placeholderText: login.initialDeviceName()
                toolTipText: qsTr("A name for this device which will be shown to others when verifying your devices. If nothing is provided, a default is used.")
                Keys.forwardTo: [pwBtn, ssoRepeater]
            }

            MatrixTextField {
                id: hsLabel
                enabled: visible
                visible: login.homeserverNeeded

                Layout.fillWidth: true
                label: qsTr("Homeserver address")
                placeholderText: qsTr("yourserver.example.com:8787")
                text: login.homeserver
                onEditingFinished: login.homeserver = text
                toolTipText: qsTr("The address that can be used to contact your homeserver's client API.\nExample: https://yourserver.example.com:8787")
                Keys.forwardTo: [pwBtn, ssoRepeater]
            }

            Item {
                Layout.preferredHeight: Komai.listIconSize
                Layout.fillWidth: true

                Spinner {
                    height: parent.height
                    anchors.centerIn: parent

                    visible: running
                    running: login.loggingIn
                    foreground: palette.mid
                }
            }

            MatrixText {
                Layout.fillWidth: true
                textFormat: Text.PlainText
                color: Komai.theme.error
                text: loginPage.error
                visible: text
                wrapMode: TextEdit.Wrap
            }

            MatrixText {
                Layout.fillWidth: true
                textFormat: Text.PlainText
                color: palette.buttonText
                visible: !loginPage.error && (login.lookingUpHs || loginPage.hasPendingLoginInput || (login.homeserverNeeded && !login.homeserverValid))
                text: login.lookingUpHs ? qsTr("Checking homeserver...")
                                       : (loginPage.hasPendingLoginInput ? qsTr("Finish editing the login fields to continue.")
                                                                         : qsTr("Login is disabled until the homeserver address is valid."))
                wrapMode: TextEdit.Wrap
            }

            KomaiButton {
                id: pwBtn
                visible: login.passwordSupported
                enabled: loginPage.loginEnabled
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Login")
                icon.source: "qrc:/icons/icons/ui/arrow-right.svg"
                highlighted: true
                function pwLogin() {
                    login.onLoginButtonClicked(Login.Password, matrixIdLabel.text, passwordLabel.text, deviceNameLabel.text)
                }
                onClicked: pwBtn.pwLogin()
                Keys.onEnterPressed: pwBtn.pwLogin()
                Keys.onReturnPressed: pwBtn.pwLogin()
                Keys.enabled: pwBtn.enabled && login.passwordSupported
            }

            Repeater {
                id: ssoRepeater

                model: login.identityProviders

                delegate: KomaiButton {
                    id: ssoBtn
                    visible: login.ssoSupported
                    enabled: loginPage.loginEnabled
                    Layout.alignment: Qt.AlignHCenter
                    text: modelData.name
                    icon.source: modelData.avatarUrl.replace("mxc://", "image://MxcImage/")
                    function ssoLogin() {
                        login.onLoginButtonClicked(Login.SSO, matrixIdLabel.text, modelData.id, deviceNameLabel.text)
                    }
                    onClicked: ssoBtn.ssoLogin()
                    Keys.onEnterPressed: ssoBtn.ssoLogin()
                    Keys.onReturnPressed: ssoBtn.ssoLogin()
                    Keys.enabled: ssoBtn.enabled && !login.passwordSupported
                }
            }
    }

    ImageButton {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: Komai.paddingMedium
        width: Komai.listIconSize
        height: Komai.listIconSize
        image: ":/icons/icons/ui/angle-arrow-left.svg"
        toolTipVisible: hovered
        toolTipText: qsTr("Back")
        onClicked: mainWindow.pop()
    }
}
