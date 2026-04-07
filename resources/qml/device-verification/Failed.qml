// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../components" as Components
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai 1.0

ColumnLayout {
    property string title: qsTr("Verification Failed")
    spacing: 16

    Label {
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        text: {
            switch (flow.error) {
                case DeviceVerificationFlow.UnknownMethod:
                return qsTr("The other client does not support this verification method.");
                case DeviceVerificationFlow.MismatchedCommitment:
                case DeviceVerificationFlow.MismatchedSAS:
                case DeviceVerificationFlow.KeyMismatch:
                return qsTr("Key mismatch detected!");
                case DeviceVerificationFlow.Timeout:
                return qsTr("Device verification timed out.");
                case DeviceVerificationFlow.User:
                return qsTr("The other party canceled the verification.");
                case DeviceVerificationFlow.AcceptedOnOtherDevice:
                return qsTr("The verification was accepted by a different device.");
                case DeviceVerificationFlow.OutOfOrder:
                return qsTr("Verification messages received out of order!");
                default:
                return qsTr("Unknown verification error.");
            }
        }
        color: palette.text
    }

    RowLayout {
        Item {
            Layout.fillWidth: true
        }

        Components.KomaiButton {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Close")
            onClicked: dialog.close()
        }

    }

}
