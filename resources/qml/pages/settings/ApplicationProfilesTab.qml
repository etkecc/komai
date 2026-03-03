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
        anchors.margins: Komai.paddingLarge
        standalone: false
    }
}
