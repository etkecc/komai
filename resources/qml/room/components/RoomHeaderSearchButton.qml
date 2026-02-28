// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls

RoomHeaderActionButton {
    id: root

    required property var room

    property bool searchActive: false

    ToolTip.text: qsTr("Search this room")
    image: ":/icons/icons/ui/search.svg"
    visible: !!room

    onClicked: searchActive = !searchActive
}
