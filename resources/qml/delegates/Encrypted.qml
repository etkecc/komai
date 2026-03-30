// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../components" as Components
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15
import cc.etke.komai 1.0

Control {
    id: r

    required property int encryptionError
    required property string eventId
    required property QtObject styleProfile
    readonly property bool canRequestKey: !!room && typeof room.requestKeyForEvent === "function"

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
        }

        ColumnLayout {
            spacing: Komai.paddingSmall
            Layout.fillWidth: true

            Label {
                id: encryptedText
                text: r.canRequestKey
                    ? qsTr("This message couldn't be decrypted. The app requested the key automatically, but you can try requesting it again.")
                    : qsTr("This message couldn't be decrypted.")
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
