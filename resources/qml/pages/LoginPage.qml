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
    property int maxExpansion: 800

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
                source: "qrc:/logos/komai.svg"
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

            // ── Card 1: Matrix ID ──
            Item {
                Layout.fillWidth: true
                implicitHeight: mxidRow.implicitHeight

                HoverHandler { id: mxidHover; blocking: false }
                Rectangle { anchors.fill: mxidRow; color: mxidHover.hovered ? palette.dark : palette.window; radius: Komai.paddingMedium; z: -1 }

                RowLayout {
                    id: mxidRow
                    width: parent.width
                    spacing: Komai.paddingSmall

                    Label {
                        Layout.fillWidth: true
                        Layout.margins: Komai.paddingMedium
                        text: qsTr("Matrix ID")
                        color: mxidHover.hovered ? palette.brightText : palette.text
                    }

                    Spinner {
                        Layout.preferredHeight: matrixIdLabel.height / 2
                        Layout.alignment: Qt.AlignVCenter
                        visible: running
                        running: login.lookingUpHs
                        foreground: palette.mid
                    }

                    KomaiTextField {
                        id: matrixIdLabel
                        Layout.fillWidth: true
                        Layout.minimumWidth: 300
                        Layout.topMargin: Komai.paddingSmall
                        Layout.bottomMargin: Komai.paddingSmall
                        Layout.rightMargin: Komai.paddingSmall
                        placeholderText: qsTr("e.g @user:server.example")
                        Keys.forwardTo: [pwBtn, ssoRepeater]

                        onTextChanged: mxidDebounce.restart()

                        Timer {
                            id: mxidDebounce

                            interval: 350
                            onTriggered: login.mxid = matrixIdLabel.text
                        }
                    }
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

            // ── Card 2: Password ──
            Item {
                Layout.fillWidth: true
                implicitHeight: pwRow.implicitHeight
                visible: login.passwordSupported

                HoverHandler { id: pwHover; blocking: false }
                Rectangle { anchors.fill: pwRow; color: pwHover.hovered ? palette.dark : palette.window; radius: Komai.paddingMedium; z: -1 }

                RowLayout {
                    id: pwRow
                    width: parent.width
                    spacing: Komai.paddingSmall

                    Label {
                        Layout.fillWidth: true
                        Layout.margins: Komai.paddingMedium
                        text: qsTr("Password")
                        color: pwHover.hovered ? palette.brightText : palette.text
                    }

                    ImageButton {
                        id: showPwButton
                        Layout.preferredWidth: Math.round(Settings.uiFontSizePt * 2)
                        Layout.preferredHeight: Math.round(Settings.uiFontSizePt * 2)
                        Layout.alignment: Qt.AlignVCenter
                        buttonTextColor: pwHover.hovered ? palette.brightText : palette.buttonText
                        image: passwordLabel.echoMode === TextInput.Password ? ":/icons/icons/ui/eye-show.svg" : ":/icons/icons/ui/eye-hide.svg"
                        toolTipVisible: hovered
                        toolTipText: qsTr("Show/Hide Password")
                        onClicked: {
                            passwordLabel.echoMode = passwordLabel.echoMode === TextInput.Normal
                                ? TextInput.Password : TextInput.Normal
                        }
                    }

                    KomaiTextField {
                        id: passwordLabel
                        Layout.fillWidth: true
                        Layout.minimumWidth: 300
                        Layout.topMargin: Komai.paddingSmall
                        Layout.bottomMargin: Komai.paddingSmall
                        Layout.rightMargin: Komai.paddingSmall
                        echoMode: TextInput.Password
                        Keys.forwardTo: [pwBtn, ssoRepeater]
                    }
                }
            }

            // ── Card 3: Device name ──
            Item {
                Layout.fillWidth: true
                implicitHeight: deviceRow.implicitHeight

                HoverHandler { id: deviceHover; blocking: false }
                Rectangle { anchors.fill: deviceRow; color: deviceHover.hovered ? palette.dark : palette.window; radius: Komai.paddingMedium; z: -1 }

                RowLayout {
                    id: deviceRow
                    width: parent.width
                    spacing: Komai.paddingSmall

                    Label {
                        Layout.fillWidth: true
                        Layout.margins: Komai.paddingMedium
                        text: qsTr("Device name")
                        color: deviceHover.hovered ? palette.brightText : palette.text
                    }

                    KomaiTextField {
                        id: deviceNameLabel
                        Layout.fillWidth: true
                        Layout.minimumWidth: 300
                        Layout.topMargin: Komai.paddingSmall
                        Layout.bottomMargin: Komai.paddingSmall
                        Layout.rightMargin: Komai.paddingSmall
                        Keys.forwardTo: [pwBtn, ssoRepeater]
                    }
                }
            }

            // ── Card 4: Homeserver (conditional) ──
            Item {
                Layout.fillWidth: true
                implicitHeight: hsRow.implicitHeight
                visible: login.homeserverNeeded

                HoverHandler { id: hsHover; blocking: false }
                Rectangle { anchors.fill: hsRow; color: hsHover.hovered ? palette.dark : palette.window; radius: Komai.paddingMedium; z: -1 }

                RowLayout {
                    id: hsRow
                    width: parent.width
                    spacing: Komai.paddingSmall

                    Label {
                        Layout.fillWidth: true
                        Layout.margins: Komai.paddingMedium
                        text: qsTr("Homeserver")
                        color: hsHover.hovered ? palette.brightText : palette.text
                    }

                    KomaiTextField {
                        id: hsLabel
                        Layout.fillWidth: true
                        Layout.minimumWidth: 300
                        Layout.topMargin: Komai.paddingSmall
                        Layout.bottomMargin: Komai.paddingSmall
                        Layout.rightMargin: Komai.paddingSmall
                        placeholderText: qsTr("yourserver.example.com:8787")
                        text: login.homeserver
                        Keys.forwardTo: [pwBtn, ssoRepeater]

                        onTextChanged: hsDebounce.restart()

                        Timer {
                            id: hsDebounce

                            interval: 350
                            onTriggered: login.homeserver = hsLabel.text
                        }
                    }
                }
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
