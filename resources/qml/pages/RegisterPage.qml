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
    id: registrationPage
    property int maxExpansion: 600

    property string error: regis.error

    Registration {
        id: regis
    }

    Onboarding.OnboardingScrollPage {
        id: scroll
        anchors.fill: parent
        maxContentWidth: registrationPage.maxExpansion
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
                Layout.bottomMargin: 0
                Layout.alignment: Qt.AlignHCenter
                Layout.fillWidth: true
                text: qsTr("Register a Matrix account")
                color: palette.text
                font.pointSize: Settings.uiFontSizePt * 1.5
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
            }

            Label {
                Layout.topMargin: Komai.paddingSmall
                Layout.leftMargin: Komai.paddingLarge
                Layout.rightMargin: Komai.paddingLarge
                Layout.bottomMargin: Komai.paddingMedium
                Layout.alignment: Qt.AlignHCenter
                Layout.fillWidth: true
                text: qsTr("But... where?")
                color: palette.buttonText
                font.pointSize: Settings.uiFontSizePt
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
            }

            // Informational guide
            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingMedium
                Layout.rightMargin: Komai.paddingMedium
                Layout.topMargin: Komai.paddingSmall
                Layout.bottomMargin: Komai.paddingMedium
                color: palette.alternateBase
                radius: 8
                implicitHeight: guideColumn.implicitHeight + Komai.paddingMedium * 2

                ColumnLayout {
                    id: guideColumn
                    anchors.fill: parent
                    anchors.margins: Komai.paddingMedium
                    spacing: Komai.paddingSmall

                    // Public servers
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Komai.paddingMedium

                        Image {
                            Layout.preferredHeight: 20
                            Layout.preferredWidth: 20
                            Layout.alignment: Qt.AlignTop
                            source: "image://colorimage/:/icons/icons/ui/world.svg?" + palette.buttonText
                            sourceSize.height: 20
                            sourceSize.width: 20
                        }

                        Text {
                            Layout.fillWidth: true
                            textFormat: Text.RichText
                            wrapMode: Text.Wrap
                            font.pointSize: Settings.uiFontSizePt * 0.95
                            text: "<style>a { color: " + palette.highlight + "; }</style>" +
                                  qsTr("Public servers like <a href=\"https://matrix.org/\">matrix.org</a> exist (may be overloaded)")
                            color: palette.text
                            onLinkActivated: function(link) { Qt.openUrlExternally(link); }

                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.NoButton
                                cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
                            }
                        }
                    }

                    // Hosting providers
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Komai.paddingMedium

                        Image {
                            Layout.preferredHeight: 20
                            Layout.preferredWidth: 20
                            Layout.alignment: Qt.AlignTop
                            source: "image://colorimage/:/icons/icons/ui/building-shop.svg?" + palette.buttonText
                            sourceSize.height: 20
                            sourceSize.width: 20
                        }

                        Text {
                            Layout.fillWidth: true
                            textFormat: Text.RichText
                            wrapMode: Text.Wrap
                            font.pointSize: Settings.uiFontSizePt * 0.95
                            text: "<style>a { color: " + palette.highlight + "; }</style>" +
                                  qsTr("<a href=\"https://matrix.org/ecosystem/hosting/\">Hosting providers</a> exist, including the makers of this app — <a href=\"https://etke.cc/\">etke.cc</a>")
                            color: palette.text
                            onLinkActivated: function(link) { Qt.openUrlExternally(link); }

                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.NoButton
                                cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
                            }
                        }
                    }

                    // Self-hosting
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Komai.paddingMedium

                        Image {
                            Layout.preferredHeight: 20
                            Layout.preferredWidth: 20
                            Layout.alignment: Qt.AlignTop
                            source: "image://colorimage/:/icons/icons/ui/settings.svg?" + palette.buttonText
                            sourceSize.height: 20
                            sourceSize.width: 20
                        }

                        Text {
                            Layout.fillWidth: true
                            textFormat: Text.RichText
                            wrapMode: Text.Wrap
                            font.pointSize: Settings.uiFontSizePt * 0.95
                            text: "<style>a { color: " + palette.highlight + "; }</style>" +
                                  qsTr("<a href=\"https://matrix.org/ecosystem/hosting/\">Self-hosting</a> is possible (hardware or cloud infra required)")
                            color: palette.text
                            onLinkActivated: function(link) { Qt.openUrlExternally(link); }

                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.NoButton
                                cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
                            }
                        }
                    }

                    // Warning about migration
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: Komai.paddingMedium

                        Image {
                            Layout.preferredHeight: 20
                            Layout.preferredWidth: 20
                            Layout.alignment: Qt.AlignTop
                            source: "image://colorimage/:/icons/icons/ui/pin.svg?" + palette.buttonText
                            sourceSize.height: 20
                            sourceSize.width: 20
                        }

                        Text {
                            Layout.fillWidth: true
                            textFormat: Text.RichText
                            wrapMode: Text.Wrap
                            font.pointSize: Settings.uiFontSizePt * 0.95
                            text: "<style>a { color: " + palette.highlight + "; }</style>" +
                                  qsTr("<a href=\"https://matrix.org/\">Matrix</a> does not support server migration yet — choose carefully")
                            color: palette.text
                            onLinkActivated: function(link) { Qt.openUrlExternally(link); }

                            MouseArea {
                                anchors.fill: parent
                                acceptedButtons: Qt.NoButton
                                cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
                            }
                        }
                    }
                }
            }

            RowLayout {
                spacing: Komai.paddingLarge

                Layout.fillWidth: true
                MatrixTextField {
                    id: hsLabel
                    label: qsTr("Homeserver")
                    placeholderText: qsTr("your.server")
                    onEditingFinished: regis.setServer(text)

                    ToolTip.text: qsTr("The server address where you want to create your account")
                }


                Spinner {
                    Layout.preferredHeight: hsLabel.height/2
                    Layout.alignment: Qt.AlignBottom

                    visible: running
                    running: regis.lookingUpHs
                    foreground: palette.mid
                }
            }

            MatrixText {
                Layout.fillWidth: true
                textFormat: Text.PlainText
                color: Komai.theme.error
                text: regis.hsError
                visible: text
                wrapMode: TextEdit.Wrap
            }

            RowLayout {
                spacing: Komai.paddingLarge

                visible: regis.supported

                Layout.fillWidth: true
                MatrixTextField {
                    id: usernameLabel
                    Layout.fillWidth: true
                    label: qsTr("Username")
                    ToolTip.text: qsTr("The username must not be empty, and must contain only the characters a-z, 0-9, ., _, =, -, and /.")
                    onEditingFinished: regis.checkUsername(text)
                }
                Spinner {
                    Layout.preferredHeight: usernameLabel.height/2
                    Layout.alignment: Qt.AlignBottom

                    visible: running
                    running: regis.lookingUpUsername
                    foreground: palette.mid
                }

                Image {
                    Layout.preferredHeight: usernameLabel.height/2
                    Layout.preferredWidth: usernameLabel.height/2
                    Layout.alignment: Qt.AlignBottom
                    source: regis.usernameAvailable ? ("image://colorimage/:/icons/icons/ui/checkmark.svg?" + Komai.theme.success) : ("image://colorimage/:/icons/icons/ui/dismiss.svg?" + Komai.theme.error)
                    visible: regis.usernameAvailable || regis.usernameUnavailable
                    ToolTip.visible: ma.hovered
                    ToolTip.text: qsTr("Back")
                    sourceSize.height: height
                    sourceSize.width: width
                    HoverHandler {
                        id: ma
                    }
                }
            }

            MatrixText {
                Layout.fillWidth: true
                textFormat: Text.PlainText
                color: Komai.theme.error
                text: regis.usernameError
                visible: text && regis.supported
                wrapMode: TextEdit.Wrap
            }


            MatrixTextField {
                visible: regis.supported
                id: passwordLabel
                Layout.fillWidth: true
                label: qsTr("Password")
                echoMode: TextInput.Password
                ToolTip.text: qsTr("Please choose a secure password. The exact requirements for password strength may depend on your server.")
            }

            MatrixTextField {
                visible: regis.supported
                id: passwordConfirmationLabel
                Layout.fillWidth: true
                label: qsTr("Password confirmation")
                echoMode: TextInput.Password
            }

            MatrixText {
                Layout.fillWidth: true
                visible: regis.supported
                textFormat: Text.PlainText
                color: Komai.theme.error
                text: passwordLabel.text != passwordConfirmationLabel.text ? qsTr("Your passwords do not match!") : ""
                wrapMode: TextEdit.Wrap
            }

            MatrixTextField {
                visible: regis.supported
                id: deviceNameLabel
                Layout.fillWidth: true
                label: qsTr("Device name")
                placeholderText: regis.initialDeviceName()
                ToolTip.text: qsTr("A name for this device which will be shown to others when verifying your devices. If nothing is provided a default is used.")
            }

            Item {
                Layout.preferredHeight: Komai.listIconSize
                Layout.fillWidth: true

                Spinner {
                    height: parent.height
                    anchors.centerIn: parent

                    visible: running
                    running: regis.registering
                    foreground: palette.mid
                }
            }

            MatrixText {
                Layout.fillWidth: true
                textFormat: Text.PlainText
                color: Komai.theme.error
                text: registrationPage.error
                visible: text
                wrapMode: TextEdit.Wrap
            }

            KomaiButton {
                id: regisBtn
                visible: regis.supported
                enabled: usernameLabel.text && passwordLabel.text && passwordLabel.text == passwordConfirmationLabel.text
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Register")
                icon.source: "qrc:/icons/icons/ui/plus-circle.svg"
                highlighted: true
                function register() {
                    regis.startRegistration(usernameLabel.text, passwordLabel.text, deviceNameLabel.text)
                }
                onClicked: regisBtn.register()
                Keys.onEnterPressed: regisBtn.register()
                Keys.onReturnPressed: regisBtn.register()
                Keys.enabled: regisBtn.enabled && regis.supported
            }
    }

    ImageButton {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: Komai.paddingMedium
        width: Komai.listIconSize
        height: Komai.listIconSize
        image: ":/icons/icons/ui/angle-arrow-left.svg"
        ToolTip.visible: hovered
        ToolTip.text: qsTr("Back")
        onClicked: mainWindow.pop()
    }
}
