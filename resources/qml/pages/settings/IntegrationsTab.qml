// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import cc.etke.komai

SettingsContent {
    tabFilter: UserSettingsModel.TabIntegrations

    // Section IDs for the search proxy: each ties this tab's custom
    // header/footer slots to a keyword bucket so search can hide them
    // independently.
    headerSectionId: "transcription"
    footerSectionId: "browser"

    // Voice transcription gets its own headerContent slot so the section
    // sits at the very top of the Integrations tab, above the model-row
    // sections (D-Bus, Matrix Rooms Search, Browser).
    headerContent: Component {
        Loader {
            Layout.fillWidth: true
            Layout.preferredHeight: item ? item.implicitHeight : 0
            source: "IntegrationsTab/TranscriptionSetting.qml"
        }
    }

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
