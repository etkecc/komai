// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
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
            source: "qrc:/resources/qml/pages/settings/IntegrationsTab/BrowserCommandSetting.qml"
        }
    }
}
