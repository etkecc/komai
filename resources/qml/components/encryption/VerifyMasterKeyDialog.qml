// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import ".." as Components
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import im.nheko 1.0

Components.OverlayDialog {
    id: root

    property bool hasSSSS: false
    property bool canVerifyWithAnotherDevice: false
    signal verifyWithAnotherDevice()
    signal unlockKeyBackup()
    signal resetIdentity()

    titleText: qsTr("Activate Encryption")
    titleIcon: ":/icons/icons/ui/shield-regular-exclamation-mark.svg"
    titleIconColor: palette.text

    TextEdit {
        Layout.fillWidth: true
        color: palette.text
        readOnly: true
        selectByMouse: true
        text: qsTr("This account already has encryption keys, but this device is not verified yet.\nVerification marks this device as trusted and gives you access to encrypted messages.")
        textFormat: TextEdit.PlainText
        wrapMode: TextEdit.Wrap
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Nheko.paddingSmall

        Button {
            Layout.rightMargin: Nheko.paddingLarge
            text: qsTr("Not now")
            onClicked: root.close()
        }

        Item {
            Layout.fillWidth: true
        }

        Button {
            icon.source: "qrc:/icons/icons/ui/refresh.svg"
            icon.width: 18
            icon.height: 18
            text: qsTr("Reset identity")

            onClicked: resetIdentity()
        }

        Button {
            icon.source: "qrc:/icons/icons/ui/shield-regular-checkmark.svg"
            icon.width: 18
            icon.height: 18
            text: qsTr("Verify with another device")
            // Keep this visible-but-disabled when no candidates exist:
            // verification with another device is still a valid route in principle, but
            // currently unavailable (e.g. no other signed-in verifiable device found).
            enabled: root.canVerifyWithAnotherDevice
            ToolTip.text: qsTr("No other signed-in device is currently available for verification.")
            ToolTip.visible: hovered && !enabled

            onClicked: verifyWithAnotherDevice()
        }

        Button {
            icon.source: "qrc:/icons/icons/ui/key.svg"
            icon.width: 18
            icon.height: 18
            text: qsTr("Unlock key backup")
            // Hide completely when backup unlock is fundamentally unavailable in this state.
            visible: root.hasSSSS

            onClicked: {
                unlockKeyBackup();
                root.close();
            }
        }
    }
}
