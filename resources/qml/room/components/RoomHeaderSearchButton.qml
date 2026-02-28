// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls

RoomHeaderActionButton {
    id: root

    required property var room
    property bool showTextLabel: false

    property bool searchActive: false

    toolTipText: qsTr("Search this room")
    image: ":/icons/icons/ui/search.svg"
    labelText: qsTr("Search")
    showLabel: showTextLabel
    visible: !!room

    onClicked: searchActive = !searchActive
}
