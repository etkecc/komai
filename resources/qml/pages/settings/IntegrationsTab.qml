// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import cc.etke.komai

SettingsContent {
    tabFilter: UserSettingsModel.TabIntegrations

    footerContent: Component {
        Loader {
            Layout.fillWidth: true
            Layout.topMargin: Komai.paddingLarge
            Layout.bottomMargin: Komai.paddingMedium
            Layout.preferredHeight: item ? item.implicitHeight : 0
            source: "IntegrationsTab/BrowserCommandSetting.qml"
        }
    }
}
