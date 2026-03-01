// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick

RoomHeaderActionButton {
    id: root

    required property var room
    property bool showTextLabel: false

    property bool searchActive: false

    alwaysShowToolTip: true
    toolTipText: qsTr("Search within this room's messages")
    image: ":/icons/icons/ui/search.svg"
    labelText: qsTr("Search")
    showLabel: showTextLabel
    visible: !!room

    onClicked: searchActive = !searchActive
}
