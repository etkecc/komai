// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components" as Components
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai 1.0

Components.OverlayDialog {
    id: joinRoomRoot

    title: qsTr("Join room")
    titleIcon: ":/icons/icons/ui/arrow-join.svg"
    initialFocusItem: input

    Components.KomaiTextField {
        id: input

        Layout.fillWidth: true
        placeholderText: qsTr("E.g. !roomID or #alias:example.com")
        onAccepted: {
            if (input.text.match(/^[#!].+?:.{3,}/)) {
                Komai.joinRoom(input.text);
                joinRoomRoot.close();
            }
        }
    }

    Components.KomaiButton {
        Layout.alignment: Qt.AlignRight
        text: qsTr("Join")
        highlighted: true
        enabled: input.text.match(/^[#!].+?:.{3,}/)
        onClicked: {
            Komai.joinRoom(input.text);
            joinRoomRoot.close();
        }
    }
}
