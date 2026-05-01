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
    readonly property bool isLoading: root.threadLoading || root.roomLoading

    anchors.fill: parent
    visible: !root.rootItem.hasTimeline
    spinning: root.isLoading
    headline: root.threadLoading
        ? qsTr("Loading thread…")
        : (root.roomLoading
            ? qsTr("Loading room…")
            : (root.rootItem.threadViewActive
                ? qsTr("No messages in this thread")
                : qsTr("No messages to display")))
    detail: root.isLoading || root.rootItem.threadViewActive
        ? ""
        : qsTr("If this looks wrong, check whether you have hidden event types in this room's settings.")
}
