// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import cc.etke.komai

RoomHeaderActionButton {
    id: root

    required property var room
    required property string roomId
    property bool showTextLabel: false

    property bool pinsShown: !Settings.hiddenPins.includes(roomId)

    toolTipText: qsTr("Show or hide pinned messages")
    image: pinsShown ? ":/icons/icons/ui/pin.svg" : ":/icons/icons/ui/pin-off.svg"
    labelText: qsTr("Pins")
    showLabel: showTextLabel
    visible: !!room && room.pinnedMessages.length > 0

    onClicked: {
        var ps = Settings.hiddenPins;
        if (pinsShown) {
            ps.push(roomId);
        } else {
            const index = ps.indexOf(roomId);
            if (index > -1) {
                ps.splice(index, 1);
            }
        }
        Settings.hiddenPins = ps;
    }
}
