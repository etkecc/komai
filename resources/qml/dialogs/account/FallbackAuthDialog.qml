// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components" as Components
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import cc.etke.komai 1.0

Components.OverlayDialog {
    id: fallbackRoot

    required property FallbackAuth fallback

    property bool wasAccepted: false

    title: qsTr("Fallback authentication")
    titleIcon: ":/icons/icons/ui/key.svg"

    onOpened: wasAccepted = false
    onClosed: {
        if (!wasAccepted)
            fallback.cancel();
    }

    Label {
        Layout.fillWidth: true
        color: palette.text
        wrapMode: Text.WordWrap
        text: qsTr("Open the fallback, follow the steps, and confirm after completing them.")
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Komai.paddingMedium

        Button {
            text: qsTr("Cancel")
            onClicked: fallbackRoot.close()
        }

        Button {
            text: qsTr("Open in Browser")
            onClicked: fallback.openFallbackAuth()
        }

        Item { Layout.fillWidth: true }

        Button {
            text: qsTr("Confirm")
            highlighted: true
            onClicked: {
                fallbackRoot.wasAccepted = true;
                fallback.confirm();
                fallbackRoot.close();
            }
        }
    }
}
