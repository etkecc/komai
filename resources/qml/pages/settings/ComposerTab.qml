// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import cc.etke.komai

SettingsContent {
    tabFilter: UserSettingsModel.TabComposer

    // Spell-checking lives at the bottom of the Composer tab, below the
    // model-driven rows (incl. Feedback).
    footerContent: Component {
        Loader {
            Layout.fillWidth: true
            Layout.topMargin: Komai.paddingLarge
            Layout.bottomMargin: Komai.paddingMedium
            Layout.preferredHeight: item ? item.implicitHeight : 0
            source: "ComposerTab/SpellcheckSetting.qml"
        }
    }
}
