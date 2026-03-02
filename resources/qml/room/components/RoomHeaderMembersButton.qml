// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import cc.etke.komai

RoomHeaderActionButton {
    id: root

    required property var room
    property bool showTextLabel: false
    readonly property int memberCount: room ? room.roomMemberCount : 0

    toolTipText: qsTr("Show room members.")
    image: ":/icons/icons/ui/people.svg"
    labelText: qsTr("%n member(s)", "", memberCount)
    showLabel: showTextLabel
    visible: !!room

    onClicked: TimelineManager.openRoomInfo(room.roomId, "members")
}
