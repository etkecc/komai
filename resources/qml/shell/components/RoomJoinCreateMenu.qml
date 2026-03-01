// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import cc.etke.komai

Menu {
    id: root

    required property var profileContextMenu

    MenuItem {
        text: qsTr("Join a room")

        onTriggered: Komai.openJoinRoomDialog()
    }
    MenuItem {
        text: qsTr("Create a new room")

        onTriggered: root.profileContextMenu.openCreateRoomDialog({})
    }
    MenuItem {
        text: qsTr("Start a direct chat")

        onTriggered: root.profileContextMenu.openCreateDirectDialog()
    }
    MenuItem {
        text: qsTr("Create a new community")

        onTriggered: root.profileContextMenu.openCreateRoomDialog({
                "space": true
            })
    }
}
