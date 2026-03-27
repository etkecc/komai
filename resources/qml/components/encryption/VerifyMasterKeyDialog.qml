// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import ".." as Components
import QtQuick 2.15
import QtQuick.Layouts 1.3
import cc.etke.komai 1.0

Components.OverlayDialog {
    id: root

    property bool hasSSSS: false
    property bool canVerifyWithAnotherDevice: false
    signal verifyWithAnotherDevice()
    signal unlockKeyBackup()
    signal resetIdentity()

    title: qsTr("Activate Encryption")
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
        spacing: Komai.paddingSmall

        Components.KomaiButton {
            Layout.rightMargin: Komai.paddingLarge
            text: qsTr("Not now")
            onClicked: root.close()
        }

        Item {
            Layout.fillWidth: true
        }

        Components.KomaiButton {
            icon.source: "qrc:/icons/icons/ui/refresh.svg"
            text: qsTr("Reset identity")

            onClicked: resetIdentity()
        }

        Components.KomaiButton {
            icon.source: "qrc:/icons/icons/ui/shield-regular-checkmark.svg"
            text: qsTr("Verify with another device")
            highlighted: true
            // Keep this visible-but-disabled when no candidates exist:
            // verification with another device is still a valid route in principle, but
            // currently unavailable (e.g. no other signed-in verifiable device found).
            enabled: root.canVerifyWithAnotherDevice
            toolTipText: qsTr("No other signed-in device is currently available for verification.")
            toolTipVisible: hovered && !enabled

            onClicked: verifyWithAnotherDevice()
        }

        Components.KomaiButton {
            icon.source: "qrc:/icons/icons/ui/key.svg"
            text: qsTr("Unlock key backup")
            highlighted: true

            onClicked: {
                unlockKeyBackup();
                root.close();
            }
        }
    }
}
