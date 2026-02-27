// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import im.nheko

SettingsContent {
    tabFilter: UserSettingsModel.TabIntegrations

    footerContent: Component {
        Loader {
            Layout.fillWidth: true
            Layout.topMargin: Nheko.paddingLarge
            Layout.bottomMargin: Nheko.paddingMedium
            Layout.preferredHeight: item ? item.implicitHeight : 0
            source: "IntegrationsTab/BrowserCommandSetting.qml"
        }
    }
}
