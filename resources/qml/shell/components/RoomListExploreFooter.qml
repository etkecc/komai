// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import cc.etke.komai

RoomListFooterBar {
    id: root

    required property var tabController

    visible: Communities.currentFilterId === ""
    actionLabel: qsTr("New")
    actionIcon: ":/icons/icons/ui/tab-add.svg"

    onActionClicked: root.tabController.openNewTab()
}
