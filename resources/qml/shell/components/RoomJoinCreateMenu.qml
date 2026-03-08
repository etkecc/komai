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
        text: qsTr("Join room")

        onTriggered: Komai.openJoinRoomDialog()
    }
    MenuSeparator {}
    MenuItem {
        text: qsTr("New room")

        onTriggered: root.profileContextMenu.openCreateRoomDialog({})
    }
    MenuItem {
        text: qsTr("New direct chat")

        onTriggered: root.profileContextMenu.openCreateDirectDialog()
    }
    MenuSeparator {}
    MenuItem {
        text: qsTr("New space")

        onTriggered: root.profileContextMenu.openCreateRoomDialog({
                "space": true
            })
    }
}
