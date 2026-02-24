// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import im.nheko

SettingsContent {
    tabFilter: UserSettingsModel.TabAbout

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

            Label {
                Layout.alignment: Qt.AlignHCenter
                Layout.fillWidth: true
                text: Nheko.tagline
                color: palette.buttonText
                font.pointSize: Settings.fontSize * 1.2
                wrapMode: Text.Wrap
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }
}
