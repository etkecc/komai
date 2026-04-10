// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import cc.etke.komai

Menu {
    id: root

    MenuItem {
        text: qsTr("Join room")

        onTriggered: Komai.openJoinRoomDialog()
    }
    MenuSeparator {}
    MenuItem {
        text: qsTr("New room")

        onTriggered: timelineRoot.openCatalogDialog(componentCatalog.roomCreateDialog, {})
    }
    MenuItem {
        text: qsTr("New direct chat")

        onTriggered: timelineRoot.openCatalogDialog(componentCatalog.roomCreateDirectDialog)
    }
    MenuSeparator {}
    MenuItem {
        text: qsTr("New space")

        onTriggered: timelineRoot.openCatalogDialog(componentCatalog.roomCreateDialog, {
                "space": true
            })
    }
}
