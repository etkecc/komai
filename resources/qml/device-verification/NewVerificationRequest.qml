// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../components" as Components
import QtQuick 2.3
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.10
import cc.etke.komai

ColumnLayout {
    property string title: flow.sender ? qsTr("Send verification request?") : qsTr("Received Verification Request")

    spacing: 16

    Label {
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        text: {
            if (flow.sender) {
                if (flow.isSelfVerification)
                    if (flow.isMultiDeviceVerification)
                        return qsTr("Some of your logged-in devices are not verified yet. Verify to unlock encrypted messages.\n\nTo start, make sure one of your other devices is available.");
                    else
                        return qsTr("This device (ID: %1) is not verified yet.\n\nVerify to unlock encrypted messages.").arg(flow.deviceId);
                else
                    return qsTr("Verify the other party to ensure your encrypted communications are secure.");
            } else {
                if (!flow.isSelfVerification && flow.isDeviceVerification)
                    return qsTr("%1 has requested to verify their device %2.").arg(flow.userId).arg(flow.deviceId);
                else if (!flow.isSelfVerification && !flow.isDeviceVerification)
                    return qsTr("%1 using the device %2 has requested to be verified.").arg(flow.userId).arg(flow.deviceId);
                else
                    return qsTr("Your device (%1) has requested to be verified.").arg(flow.deviceId);
            }
        }
        color: palette.text
    }

    RowLayout {
        Components.KomaiButton {
            Layout.alignment: Qt.AlignLeft
            text: flow.sender ? qsTr("Not now") : qsTr("Deny")
            onClicked: {
                flow.cancel();
                dialog.close();
            }
        }

        Item {
            Layout.fillWidth: true
        }

        Components.KomaiButton {
            Layout.alignment: Qt.AlignRight
            highlighted: true
            text: flow.sender ? qsTr("Start verification") : qsTr("Accept")
            onClicked: flow.next()
        }

    }

}
