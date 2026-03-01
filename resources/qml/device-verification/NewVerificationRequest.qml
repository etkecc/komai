// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.3
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.10
import cc.etke.komai

ColumnLayout {
    property string title: flow.sender ? qsTr("Send Verification Request") : qsTr("Received Verification Request")

    spacing: 16

    Label {
        // Self verification

        Layout.preferredWidth: 400
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        font.pointSize: Settings.uiFontSizePt
        text: {
            if (flow.sender) {
                if (flow.isSelfVerification)
                    if (flow.isMultiDeviceVerification)
                        return qsTr("Some of your logged-in devices are not verified yet. Verification helps protect encrypted chats and keeps key backup working.\n\nTo start, make sure one of your other devices is available.");
                    else
                        return qsTr("This device ID is: %1\n\nThis device is not verified yet. Verification helps protect encrypted chats and keeps key backup working.\n\nStart now?").arg(flow.deviceId);
                else
                    return qsTr("To ensure that no malicious user can eavesdrop on your encrypted communications you can verify the other party.");
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
        verticalAlignment: Text.AlignVCenter
    }

    Item { Layout.fillHeight: true; }

    RowLayout {
        Button {
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

        Button {
            Layout.alignment: Qt.AlignRight
            text: flow.sender ? qsTr("Start verification") : qsTr("Accept")
            onClicked: flow.next()
        }

    }

}
