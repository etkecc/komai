// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import cc.etke.komai

SettingsContent {
    tabFilter: UserSettingsModel.TabTimeline

    // Section ID for the search proxy: ties the StateEventsSection footer
    // to its keyword bucket so search can hide it when a query (e.g.
    // "compact") doesn't match its content.
    footerSectionId: "stateEvents"

    footerContent: Component {
        Loader {
            Layout.fillWidth: true
            Layout.preferredHeight: item ? item.implicitHeight : 0
            source: "TimelineTab/StateEventsSection.qml"
        }
    }
}
