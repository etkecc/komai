// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

RoomHeaderActionButton {
    id: roomLeaveButton

    property bool roomAvailable: false
    property string roomId: ""
    property bool showTextLabel: false

    toolTipText: qsTr("Leave this room")
    alwaysShowToolTip: true
    image: ":/icons/icons/ui/power-off.svg"
    labelText: qsTr("Leave")
    showLabel: showTextLabel
    visible: roomAvailable

    onClicked: TimelineManager.openLeaveRoomDialog(roomId)
}
