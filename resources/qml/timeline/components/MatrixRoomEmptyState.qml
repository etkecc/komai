// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import cc.etke.komai
import "../../ui/"

LoadingSplash {
    id: root

    required property var rootItem

    readonly property bool threadLoading: root.rootItem.threadViewActive && root.rootItem.threadTimelineLoading
    readonly property bool roomLoading: !root.rootItem.threadViewActive && root.rootItem.loading

    anchors.fill: parent
    visible: !root.rootItem.hasTimeline
    spinning: root.threadLoading || root.roomLoading
    headline: root.threadLoading
        ? qsTr("Loading thread…")
        : (root.roomLoading ? qsTr("Loading room…") : "")
    detail: ""
}
