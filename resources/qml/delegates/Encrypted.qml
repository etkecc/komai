// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../components" as Components
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai 1.0

Control {
    id: r

    required property string eventId
    required property string utdCause
    required property QtObject styleProfile
    readonly property bool canRequestKey: false

    readonly property string explanation: {
        switch (r.utdCause) {
        case "sent_before_we_joined":
            return qsTr("You weren't in the room when this message was sent.");
        case "verification_violation":
            return qsTr("This message couldn't be decrypted because the sender's identity is no longer verified.");
        case "unsigned_device":
            return qsTr("This message was sent from a device that isn't signed by its owner.");
        case "unknown_device":
            return qsTr("This message was sent from a device we couldn't securely identify.");
        case "historical_message_and_backup_disabled":
            return qsTr("History isn't available on this device. Turn on key backup to access older messages.");
        case "historical_message_and_device_unverified":
            return qsTr("Verify this device to access messages sent before it was added to your account.");
        case "withheld_for_unverified_or_insecure_device":
            return qsTr("The sender's security settings prevented sharing encryption keys with this device.");
        case "withheld_by_sender":
            return qsTr("The sender didn't share the encryption keys with this device.");
        default:
            return qsTr("This message couldn't be decrypted.");
        }
    }

    padding: Komai.paddingMedium
    implicitHeight: contents.implicitHeight + Komai.paddingMedium * 2
    Layout.maximumWidth: contents.Layout.maximumWidth + padding * 2
    Layout.fillWidth: true

    contentItem: RowLayout {
        id: contents

        spacing: Komai.paddingMedium

        Image {
            source: "image://colorimage/:/icons/icons/ui/shield-regular-cross.svg?" + Komai.theme.error
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: 24
            Layout.preferredHeight: 24
            sourceSize.width: 24
            sourceSize.height: 24
        }

        ColumnLayout {
            spacing: Komai.paddingSmall
            Layout.fillWidth: true

            Label {
                id: encryptedText
                text: r.explanation
                textFormat: Text.PlainText
                wrapMode: Label.WordWrap
                color: palette.text
                Layout.fillWidth: true
                Layout.maximumWidth: implicitWidth + 1
            }

            Components.KomaiButton {
                visible: r.canRequestKey
                text: qsTr("Request key")
                onClicked: room.requestKeyForEvent(eventId)
            }

        }

    }

    background: Rectangle {
        color: palette.alternateBase
        radius: fontMetrics.lineSpacing / 2 + 2 * Komai.paddingMedium
        visible: styleProfile.showEncryptedMessageBackground
    }
}
