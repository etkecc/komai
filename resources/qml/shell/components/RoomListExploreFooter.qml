// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import cc.etke.komai

RoomListFooterBar {
    id: root

    required property var timelineRoot

    visible: Communities.currentFilterId === ""
    actionLabel: qsTr("Explore")
    actionIcon: ":/icons/icons/ui/compass-northwest.svg"

    onActionClicked: root.timelineRoot.openRoomDirectory()
}
