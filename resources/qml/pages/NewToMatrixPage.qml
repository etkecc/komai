// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai 1.0
import "onboarding" as Onboarding
import "welcome" as Welcome
import "../components/"

Rectangle {
    id: root
    property int maxExpansion: 900
    property int headerIconSize: Komai.iconSize

    color: palette.alternateBase

    readonly property string matrixOrgUrl: "https://matrix.org/"
    readonly property string homeserverUrl: "https://matrix.org/docs/matrix-concepts/elements-of-matrix/#homeserver"
    readonly property string federationUrl: "https://matrix.org/docs/older/faq/#what-does-federated-mean"
    readonly property string etkeUrl: "https://etke.cc/?utm_source=komai&utm_medium=app&utm_campaign=new-to-matrix/managed-hosting"
    readonly property string hostingUrl: "https://matrix.org/ecosystem/hosting/"
    readonly property string mdadUrl: "https://github.com/spantaleev/matrix-docker-ansible-deploy"
    readonly property string distributionsUrl: "https://matrix.org/ecosystem/distributions/"

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        // ── Header bar ──
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: Komai.navigationRowHeight
            color: palette.alternateBase

            ItemDelegate {
                id: headerBack
                anchors.left: parent.left
                height: parent.height
                topPadding: 0
                bottomPadding: 0
                leftPadding: Komai.paddingMedium
                rightPadding: Komai.paddingMedium

                HoverHandler { cursorShape: Qt.PointingHandCursor }

                background: Rectangle {
                    color: headerBack.hovered ? palette.dark : "transparent"
                }

                onClicked: mainWindow.pop()

                contentItem: RowLayout {
                    spacing: Komai.paddingSmall

                    Image {
                        Layout.preferredWidth: 24
                        Layout.preferredHeight: 24
                        Layout.alignment: Qt.AlignVCenter
                        source: "image://colorimage/:/icons/icons/ui/angle-arrow-left.svg?" + (headerBack.hovered ? palette.brightText : palette.text)
                        sourceSize.width: 24
                        sourceSize.height: 24
                    }

                    Label {
                        Layout.alignment: Qt.AlignVCenter
                        text: qsTr("Back")
                        font.pointSize: Settings.uiFontSizePt
                        font.bold: true
                        color: headerBack.hovered ? palette.brightText : palette.text
                    }
                }
            }

            RowLayout {
                anchors.centerIn: parent
                spacing: Komai.paddingMedium

                Image {
                    Layout.preferredWidth: root.headerIconSize
                    Layout.preferredHeight: root.headerIconSize
                    Layout.alignment: Qt.AlignVCenter
                    source: "qrc:/logos/komai.svg"
                    sourceSize.width: root.headerIconSize
                    sourceSize.height: root.headerIconSize
                }

                Label {
                    Layout.alignment: Qt.AlignVCenter
                    text: qsTr("New to Matrix?")
                    font.pointSize: Settings.uiFontSizePt * 1.2
                    font.bold: true
                    color: palette.text
                }
            }
        }

        // ── Separator ──
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 1
            color: palette.mid
        }

        // ── Content ──
        Onboarding.OnboardingScrollPage {
            Layout.fillWidth: true
            Layout.fillHeight: true
            maxContentWidth: root.maxExpansion
            topSpacerHeight: Komai.paddingLarge

            // ── What is Matrix? ──
            Label {
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingLarge
                Layout.rightMargin: Komai.paddingLarge
                text: qsTr("What is Matrix?")
                color: palette.text
                font.pointSize: Settings.uiFontSizePt * 1.5
                font.bold: true
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingLarge
                Layout.rightMargin: Komai.paddingLarge
                Layout.topMargin: Komai.paddingSmall
                color: palette.window
                radius: Komai.paddingSmall
                implicitHeight: introColumn.implicitHeight + Komai.paddingLarge * 2

                ColumnLayout {
                    id: introColumn
                    anchors.fill: parent
                    anchors.margins: Komai.paddingLarge
                    spacing: Komai.paddingMedium

                    Welcome.WelcomeRichText {
                        Layout.fillWidth: true
                        text: "<style>a { color: " + palette.highlight + "; }</style>" +
                              qsTr("<a href=\"%1\">Matrix</a> is an open communication network - like email, but for real-time messaging (chat).\n").arg(root.matrixOrgUrl)
                        color: palette.buttonText
                        font.pointSize: Settings.uiFontSizePt * 1.1
                    }

                    Welcome.WelcomeRichText {
                        Layout.fillWidth: true
                        text: "<style>a { color: " + palette.highlight + "; }</style>" +
                              qsTr("Your profile and message data is hosted on something called a <a href=\"%1\">homeserver</a> (your home on the network), but you can talk to anyone on any other server.").arg(root.homeserverUrl)
                        color: palette.buttonText
                        font.pointSize: Settings.uiFontSizePt * 1.1
                    }

                    Welcome.WelcomeRichText {
                        Layout.fillWidth: true
                        text: "<style>a { color: " + palette.highlight + "; }</style>" +
                              qsTr("Homeservers connect to each other via <a href=\"%1\">Matrix Federation</a>. Unlike centralized apps, no single company controls everything - you pick what works for you.").arg(root.federationUrl)
                        color: palette.buttonText
                        font.pointSize: Settings.uiFontSizePt * 1.1
                    }
                }
            }

            // ── Choose your path ──
            Label {
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingLarge
                Layout.rightMargin: Komai.paddingLarge
                Layout.topMargin: Komai.paddingLarge
                text: qsTr("Choose your path")
                color: palette.text
                font.pointSize: Settings.uiFontSizePt * 1.5
                font.bold: true
            }

            // ── Existing servers ──
            Label {
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingLarge
                Layout.rightMargin: Komai.paddingLarge
                Layout.topMargin: Komai.paddingSmall
                text: qsTr("Existing servers")
                color: palette.text
                font.pointSize: Settings.uiFontSizePt * 1.2
                font.bold: true
            }

            ItemDelegate {
                id: publicCard
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingLarge
                Layout.rightMargin: Komai.paddingLarge
                Layout.topMargin: Komai.paddingSmall
                padding: Komai.paddingMedium

                HoverHandler { cursorShape: Qt.PointingHandCursor }

                background: Rectangle {
                    radius: Komai.paddingSmall
                    color: publicCard.hovered ? palette.dark : palette.window
                    border.color: palette.mid
                    border.width: 1
                }

                onClicked: mainWindow.push(registerPage)

                contentItem: RowLayout {
                    spacing: Komai.paddingMedium

                    Image {
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                        Layout.alignment: Qt.AlignTop
                        source: "image://colorimage/:/icons/icons/ui/globe.svg?" + palette.highlight
                        sourceSize.width: 32
                        sourceSize.height: 32
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Komai.paddingSmall / 2

                        Label {
                            text: qsTr("Public server")
                            color: publicCard.hovered ? palette.brightText : palette.text
                            font.pointSize: Settings.uiFontSizePt * 1.2
                            font.bold: true
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Free community servers, great for getting started. Some may be busy during peak hours.")
                            color: publicCard.hovered ? palette.brightText : palette.buttonText
                            font.pointSize: Settings.uiFontSizePt
                            wrapMode: Text.Wrap
                        }
                    }

                    Image {
                        Layout.preferredWidth: 16
                        Layout.preferredHeight: 16
                        Layout.alignment: Qt.AlignVCenter
                        source: "image://colorimage/:/icons/icons/ui/collapsed.svg?" + (publicCard.hovered ? palette.brightText : palette.buttonText)
                        sourceSize.width: 16
                        sourceSize.height: 16
                    }
                }
            }

            ItemDelegate {
                id: customServerCard
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingLarge
                Layout.rightMargin: Komai.paddingLarge
                Layout.topMargin: Komai.paddingSmall
                padding: Komai.paddingMedium

                HoverHandler { cursorShape: Qt.PointingHandCursor }

                background: Rectangle {
                    radius: Komai.paddingSmall
                    color: customServerCard.hovered ? palette.dark : palette.window
                    border.color: palette.mid
                    border.width: 1
                }

                onClicked: mainWindow.push(registerPage, { initialServerTab: 1 })

                contentItem: RowLayout {
                    spacing: Komai.paddingMedium

                    Image {
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                        Layout.alignment: Qt.AlignTop
                        source: "image://colorimage/:/icons/icons/ui/room-directory.svg?" + palette.highlight
                        sourceSize.width: 32
                        sourceSize.height: 32
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Komai.paddingSmall / 2

                        Label {
                            text: qsTr("Another server")
                            color: customServerCard.hovered ? palette.brightText : palette.text
                            font.pointSize: Settings.uiFontSizePt * 1.2
                            font.bold: true
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Register on a specific homeserver you already know about.")
                            color: customServerCard.hovered ? palette.brightText : palette.buttonText
                            font.pointSize: Settings.uiFontSizePt
                            wrapMode: Text.Wrap
                        }
                    }

                    Image {
                        Layout.preferredWidth: 16
                        Layout.preferredHeight: 16
                        Layout.alignment: Qt.AlignVCenter
                        source: "image://colorimage/:/icons/icons/ui/collapsed.svg?" + (customServerCard.hovered ? palette.brightText : palette.buttonText)
                        sourceSize.width: 16
                        sourceSize.height: 16
                    }
                }
            }

            // ── Self-hosting ──
            Label {
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingLarge
                Layout.rightMargin: Komai.paddingLarge
                Layout.topMargin: Komai.paddingLarge
                text: qsTr("Self-hosting")
                color: palette.text
                font.pointSize: Settings.uiFontSizePt * 1.2
                font.bold: true
            }

            ItemDelegate {
                id: mdadCard
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingLarge
                Layout.rightMargin: Komai.paddingLarge
                Layout.topMargin: Komai.paddingSmall
                padding: Komai.paddingMedium

                HoverHandler { cursorShape: Qt.PointingHandCursor }

                background: Rectangle {
                    radius: Komai.paddingSmall
                    color: mdadCard.hovered ? palette.dark : palette.window
                    border.color: palette.mid
                    border.width: 1
                }

                onClicked: Qt.openUrlExternally(root.mdadUrl)

                contentItem: RowLayout {
                    spacing: Komai.paddingMedium

                    Image {
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                        Layout.alignment: Qt.AlignTop
                        source: "image://colorimage/:/icons/icons/ui/github.svg?" + palette.highlight
                        sourceSize.width: 32
                        sourceSize.height: 32
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Komai.paddingSmall / 2

                        Label {
                            text: qsTr("matrix-docker-ansible-deploy")
                            color: mdadCard.hovered ? palette.brightText : palette.text
                            font.pointSize: Settings.uiFontSizePt * 1.2
                            font.bold: true
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("An Ansible playbook for self-hosting Matrix, by the makers of Komai.")
                            color: mdadCard.hovered ? palette.brightText : palette.buttonText
                            font.pointSize: Settings.uiFontSizePt
                            wrapMode: Text.Wrap
                        }
                    }

                    Image {
                        Layout.preferredWidth: 16
                        Layout.preferredHeight: 16
                        Layout.alignment: Qt.AlignVCenter
                        source: "image://colorimage/:/icons/icons/ui/open-externally.svg?" + (mdadCard.hovered ? palette.brightText : palette.buttonText)
                        sourceSize.width: 16
                        sourceSize.height: 16
                    }
                }
            }

            ItemDelegate {
                id: otherSelfHostCard
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingLarge
                Layout.rightMargin: Komai.paddingLarge
                Layout.topMargin: Komai.paddingSmall
                padding: Komai.paddingMedium

                HoverHandler { cursorShape: Qt.PointingHandCursor }

                background: Rectangle {
                    radius: Komai.paddingSmall
                    color: otherSelfHostCard.hovered ? palette.dark : palette.window
                    border.color: palette.mid
                    border.width: 1
                }

                onClicked: Qt.openUrlExternally(root.distributionsUrl)

                contentItem: RowLayout {
                    spacing: Komai.paddingMedium

                    Image {
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                        Layout.alignment: Qt.AlignTop
                        source: "image://colorimage/:/icons/icons/ui/globe-search.svg?" + palette.highlight
                        sourceSize.width: 32
                        sourceSize.height: 32
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Komai.paddingSmall / 2

                        Label {
                            text: qsTr("Explore other self-hosting options")
                            color: otherSelfHostCard.hovered ? palette.brightText : palette.text
                            font.pointSize: Settings.uiFontSizePt * 1.2
                            font.bold: true
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Browse Matrix server distributions and deployment tools.")
                            color: otherSelfHostCard.hovered ? palette.brightText : palette.buttonText
                            font.pointSize: Settings.uiFontSizePt
                            wrapMode: Text.Wrap
                        }
                    }

                    Image {
                        Layout.preferredWidth: 16
                        Layout.preferredHeight: 16
                        Layout.alignment: Qt.AlignVCenter
                        source: "image://colorimage/:/icons/icons/ui/open-externally.svg?" + (otherSelfHostCard.hovered ? palette.brightText : palette.buttonText)
                        sourceSize.width: 16
                        sourceSize.height: 16
                    }
                }
            }

            // ── Managed hosting ──
            Label {
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingLarge
                Layout.rightMargin: Komai.paddingLarge
                Layout.topMargin: Komai.paddingLarge
                text: qsTr("Managed hosting")
                color: palette.text
                font.pointSize: Settings.uiFontSizePt * 1.2
                font.bold: true
            }

            ItemDelegate {
                id: etkeCard
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingLarge
                Layout.rightMargin: Komai.paddingLarge
                Layout.topMargin: Komai.paddingSmall
                padding: Komai.paddingMedium

                HoverHandler { cursorShape: Qt.PointingHandCursor }

                background: Rectangle {
                    radius: Komai.paddingSmall
                    color: etkeCard.hovered ? palette.dark : palette.window
                    border.color: palette.mid
                    border.width: 1
                }

                onClicked: Qt.openUrlExternally(root.etkeUrl)

                contentItem: RowLayout {
                    spacing: Komai.paddingMedium

                    Image {
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                        Layout.alignment: Qt.AlignTop
                        source: "image://colorimage/:/icons/icons/ui/building-shop.svg?" + palette.highlight
                        sourceSize.width: 32
                        sourceSize.height: 32
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Komai.paddingSmall / 2

                        Label {
                            text: qsTr("etke.cc")
                            color: etkeCard.hovered ? palette.brightText : palette.text
                            font.pointSize: Settings.uiFontSizePt * 1.2
                            font.bold: true
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Your own server, professionally managed by the makers of Komai.")
                            color: etkeCard.hovered ? palette.brightText : palette.buttonText
                            font.pointSize: Settings.uiFontSizePt
                            wrapMode: Text.Wrap
                        }
                    }

                    Image {
                        Layout.preferredWidth: 16
                        Layout.preferredHeight: 16
                        Layout.alignment: Qt.AlignVCenter
                        source: "image://colorimage/:/icons/icons/ui/open-externally.svg?" + (etkeCard.hovered ? palette.brightText : palette.buttonText)
                        sourceSize.width: 16
                        sourceSize.height: 16
                    }
                }
            }

            ItemDelegate {
                id: otherHostingCard
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingLarge
                Layout.rightMargin: Komai.paddingLarge
                Layout.topMargin: Komai.paddingSmall
                padding: Komai.paddingMedium

                HoverHandler { cursorShape: Qt.PointingHandCursor }

                background: Rectangle {
                    radius: Komai.paddingSmall
                    color: otherHostingCard.hovered ? palette.dark : palette.window
                    border.color: palette.mid
                    border.width: 1
                }

                onClicked: Qt.openUrlExternally(root.hostingUrl)

                contentItem: RowLayout {
                    spacing: Komai.paddingMedium

                    Image {
                        Layout.preferredWidth: 32
                        Layout.preferredHeight: 32
                        Layout.alignment: Qt.AlignTop
                        source: "image://colorimage/:/icons/icons/ui/globe-search.svg?" + palette.highlight
                        sourceSize.width: 32
                        sourceSize.height: 32
                    }

                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: Komai.paddingSmall / 2

                        Label {
                            text: qsTr("Explore other hosting providers")
                            color: otherHostingCard.hovered ? palette.brightText : palette.text
                            font.pointSize: Settings.uiFontSizePt * 1.2
                            font.bold: true
                        }

                        Label {
                            Layout.fillWidth: true
                            text: qsTr("Browse the Matrix hosting provider ecosystem.")
                            color: otherHostingCard.hovered ? palette.brightText : palette.buttonText
                            font.pointSize: Settings.uiFontSizePt
                            wrapMode: Text.Wrap
                        }
                    }

                    Image {
                        Layout.preferredWidth: 16
                        Layout.preferredHeight: 16
                        Layout.alignment: Qt.AlignVCenter
                        source: "image://colorimage/:/icons/icons/ui/open-externally.svg?" + (otherHostingCard.hovered ? palette.brightText : palette.buttonText)
                        sourceSize.width: 16
                        sourceSize.height: 16
                    }
                }
            }

            // ── Migration note ──
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingLarge
                Layout.rightMargin: Komai.paddingLarge
                Layout.topMargin: Komai.paddingLarge
                spacing: Komai.paddingSmall

                Image {
                    Layout.preferredWidth: 20
                    Layout.preferredHeight: 20
                    Layout.alignment: Qt.AlignTop
                    source: "image://colorimage/:/icons/icons/ui/lightbulb.svg?" + Komai.theme.attention
                    sourceSize.width: 20
                    sourceSize.height: 20
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Matrix doesn't support migrating accounts between servers yet, so choose thoughtfully.")
                    color: Komai.theme.attention
                    font.pointSize: Settings.uiFontSizePt
                    font.italic: true
                    wrapMode: Text.Wrap
                }
            }

            // ── Already have an account? ──
            Label {
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingLarge
                Layout.rightMargin: Komai.paddingLarge
                Layout.topMargin: Komai.paddingLarge * 2
                text: qsTr("Already have an account?")
                color: palette.buttonText
                font.pointSize: Settings.uiFontSizePt * 1.1
                horizontalAlignment: Text.AlignHCenter
            }

            KomaiButton {
                Layout.alignment: Qt.AlignHCenter
                Layout.topMargin: Komai.paddingSmall
                text: qsTr("Sign in")
                icon.source: "qrc:/icons/icons/ui/arrow-right.svg"
                highlighted: true
                onClicked: mainWindow.push(loginPage)
            }
        }
    }
}
