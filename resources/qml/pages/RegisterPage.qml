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
import "../components/"
import "../ui/"

Item {
    id: registrationPage
    property int maxExpansion: 800
    readonly property string matrixUrl: "https://matrix.org/"
    readonly property string matrixHostingProvidersUrl: "https://matrix.org/ecosystem/hosting/"
    readonly property string etkeRegisterUrl: "https://etke.cc/?utm_source=komai&utm_medium=app&utm_campaign=register"

    Onboarding.OnboardingScrollPage {
        id: scroll
        anchors.fill: parent
        maxContentWidth: registrationPage.maxExpansion
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
                                  qsTr("<a href=\"https://matrix.org/ecosystem/hosting/\">Hosting providers</a> exist, including the makers of this app — <a href=\"https://etke.cc/?utm_source=komai&utm_medium=app&utm_campaign=register\">etke.cc</a>")
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

            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingMedium
                Layout.rightMargin: Komai.paddingMedium
                Layout.topMargin: Komai.paddingSmall
                Layout.bottomMargin: Komai.paddingMedium
                color: palette.window
                radius: 8
                implicitHeight: noteColumn.implicitHeight + Komai.paddingMedium * 2

                ColumnLayout {
                    id: noteColumn
                    anchors.fill: parent
                    anchors.margins: Komai.paddingMedium
                    spacing: Komai.paddingSmall

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("In-app registration is not available yet.")
                        color: palette.text
                        font.pointSize: Settings.uiFontSizePt * 1.05
                        wrapMode: Text.Wrap
                    }

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Choose a homeserver, create the account in your browser, then come back and sign in with Login.")
                        color: palette.buttonText
                        wrapMode: Text.Wrap
                    }
                }
            }

            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                Layout.leftMargin: Komai.paddingLarge
                Layout.rightMargin: Komai.paddingLarge
                Layout.bottomMargin: Komai.paddingMedium
                spacing: Komai.paddingMedium

                KomaiButton {
                    text: qsTr("Hosting providers")
                    icon.source: "qrc:/icons/icons/ui/world.svg"
                    highlighted: true
                    onClicked: Qt.openUrlExternally(registrationPage.matrixHostingProvidersUrl)
                }

                KomaiButton {
                    text: qsTr("etke.cc")
                    icon.source: "qrc:/icons/icons/ui/building-shop.svg"
                    onClicked: Qt.openUrlExternally(registrationPage.etkeRegisterUrl)
                }

                KomaiButton {
                    text: qsTr("matrix.org")
                    icon.source: "qrc:/icons/icons/ui/link.svg"
                    onClicked: Qt.openUrlExternally(registrationPage.matrixUrl)
                }
            }

            Label {
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingLarge
                Layout.rightMargin: Komai.paddingLarge
                Layout.bottomMargin: Komai.paddingLarge
                text: qsTr("After creating the account, return to the welcome screen and use Login.")
                color: palette.buttonText
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
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
