// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick.Controls
import im.nheko

RoomHeaderActionButton {
    id: root

    required property var room

    ToolTip.text: qsTr("Show room members.")
    image: ":/icons/icons/ui/people.svg"
    visible: !!room

    onClicked: TimelineManager.openRoomMembers(room)
}
