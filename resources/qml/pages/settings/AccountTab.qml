// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import im.nheko

Item {
    id: root
    property bool collapsed: false

    Loader {
        anchors.fill: parent
        sourceComponent: Settings.hasActiveSession ? accountSettingsView : signedOutView
    }

    Component {
        id: accountSettingsView
        SettingsContent {
            tabFilter: UserSettingsModel.TabAccount
            collapsed: root.collapsed
        }
    }

    Component {
        id: signedOutView
        Flickable {
            anchors.fill: parent
            contentWidth: width
            contentHeight: container.implicitHeight + Nheko.paddingLarge * 2
            clip: true

            ColumnLayout {
                id: container
                y: Nheko.paddingLarge
                width: Math.max(0, parent.width - Nheko.paddingLarge * 2)
                x: Nheko.paddingLarge
                spacing: Nheko.paddingMedium

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Account")
                    font.bold: true
                    font.pointSize: 1.2 * Settings.uiFontSizePt
                    color: palette.text
                }

                Label {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: qsTr("You are not logged in yet, so account details are unavailable.")
                    color: palette.buttonText
                    font.pointSize: Settings.uiFontSizePt
                }
            }
        }
    }
}
