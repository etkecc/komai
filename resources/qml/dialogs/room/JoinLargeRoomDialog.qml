// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components" as Components
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai 1.0

Components.OverlayDialog {
    id: joinConfirmRoot

    property string roomName: ""
    property int roomIndex: -1
    property int memberCount: 0
    property string warningText: ""

    signal confirmed(int index)

    title: qsTr("Really join %1?").arg(roomName || qsTr("this room"))
    titleIcon: ":/icons/icons/ui/people.svg"
    titleIconColor: Komai.theme.attention

    Label {
        Layout.fillWidth: true
        color: palette.text
        wrapMode: Text.WordWrap
        text: qsTr("This room has %1 members.").arg(joinConfirmRoot.memberCount.toLocaleString())
    }

    Label {
        Layout.fillWidth: true
        visible: joinConfirmRoot.warningText.length > 0
        color: Komai.theme.attention
        wrapMode: Text.WordWrap
        text: joinConfirmRoot.warningText
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Komai.paddingMedium

        Components.KomaiButton {
            text: qsTr("Cancel")
            onClicked: joinConfirmRoot.close()
        }

        Item {
            Layout.fillWidth: true
        }

        Components.KomaiButton {
            text: qsTr("Join anyway")
            highlighted: true
            onClicked: {
                joinConfirmRoot.confirmed(joinConfirmRoot.roomIndex);
                joinConfirmRoot.close();
            }
        }
    }
}
