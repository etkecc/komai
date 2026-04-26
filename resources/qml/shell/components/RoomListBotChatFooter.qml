// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import cc.etke.komai

RoomListFooterBar {
    id: root

    visible: Communities.currentFilterId === "bot"
    actionLabel: qsTr("New bot chat")
    actionIcon: ":/icons/icons/ui/person.svg"

    onActionClicked: {
        var uid = Settings.userId;
        var colonIdx = uid.indexOf(":");
        var serverName = colonIdx >= 0 ? uid.substring(colonIdx + 1) : "";
        timelineRoot.openCatalogDialog(componentCatalog.roomCreateDirectDialog, {
            "initialSearchText": "bot " + serverName
        });
    }
}
