// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components"
import QtQuick
import cc.etke.komai

Item {
    id: root

    property bool collapsed: false

    ApplicationProfilesView {
        anchors.fill: parent
        anchors.leftMargin: Komai.paddingMedium
        anchors.rightMargin: Komai.paddingMedium
        anchors.topMargin: Komai.paddingLarge
        anchors.bottomMargin: Komai.paddingLarge
        standalone: false
    }
}
