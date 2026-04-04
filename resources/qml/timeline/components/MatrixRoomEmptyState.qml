// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import cc.etke.komai
import "../../ui/"

LoadingSplash {
    id: root

    required property var rootItem

    anchors.fill: parent
    visible: !root.rootItem.hasTimeline
    spinning: root.rootItem.loading
    headline: root.rootItem.loading ? qsTr("Loading room…") : ""
    detail: ""
}
