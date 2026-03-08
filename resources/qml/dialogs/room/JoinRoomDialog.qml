// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components" as Components
import "../../ui"
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import cc.etke.komai 1.0

Components.OverlayDialog {
    id: joinRoomRoot

    title: qsTr("Join room")
    titleIcon: ":/icons/icons/ui/arrow-join.svg"
    initialFocusItem: input

    Label {
        text: qsTr("Room ID or alias")
        color: palette.text
    }

    MatrixTextField {
        id: input

        Layout.fillWidth: true
        onAccepted: {
            if (input.text.match("#.+?:.{3,}")) {
                Komai.joinRoom(input.text);
                joinRoomRoot.close();
            }
        }
    }

    Button {
        Layout.alignment: Qt.AlignRight
        text: qsTr("Join")
        highlighted: true
        enabled: input.text.match("#.+?:.{3,}")
        onClicked: {
            Komai.joinRoom(input.text);
            joinRoomRoot.close();
        }
    }
}
