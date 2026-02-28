// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick.Controls
import im.nheko

RoomHeaderActionButton {
    id: root

    required property bool showBackButton

    toolTipText: qsTr("Back to room list")
    image: ":/icons/icons/ui/angle-arrow-left.svg"
    visible: showBackButton

    onClicked: Rooms.resetCurrentRoom()
}
