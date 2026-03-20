// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../components" as Components
import "../ui"
import QtQuick 2.3
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.10
import cc.etke.komai 1.0

ColumnLayout {
    property string title: qsTr("Waiting for Other Device")
    spacing: 16

    Label {
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        text: {
            switch (flow.state) {
                case "WaitingForOtherToAccept":
                    return qsTr("Waiting for the other device to accept the verification request.");
                case "WaitingForKeys":
                    return qsTr("Waiting for the other device to continue the verification process.");
                case "WaitingForMac":
                    return qsTr("Waiting for the other device to complete the verification process.");
                default:
                    return "";
            }
        }
        color: palette.text
    }

    Spinner {
        Layout.alignment: Qt.AlignHCenter
        foreground: palette.mid
    }

    RowLayout {
        Components.KomaiButton {
            Layout.alignment: Qt.AlignLeft
            text: qsTr("Cancel")
            onClicked: {
                flow.cancel();
                dialog.close();
            }
        }

        Item {
            Layout.fillWidth: true
        }

    }

}
