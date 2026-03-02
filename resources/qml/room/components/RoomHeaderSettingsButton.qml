// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick.Controls
import cc.etke.komai

RoomHeaderActionButton {
    id: root

    required property bool roomAvailable
    required property string roomId

    ToolTip.text: qsTr("Room settings")
    image: ":/icons/icons/ui/toggles.svg"
    visible: roomAvailable

    onClicked: TimelineManager.openRoomInfo(roomId, "settings")
}
