// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import ".." as Components
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import cc.etke.komai 1.0

Components.OverlayDialog {
    id: root

    property bool completionHandled: false
    property string securityKeyExample: "EsUJ G5yL 75Sw 5cas scd8 4gSU rdVX Uuzk QsKC vYxe rTdc Hxee"
    signal unlockRequested(string value)
    signal cancelled()

    function submitUnlock()
    {
        if (unlockKeyInput.text.length === 0)
            return;

        completionHandled = true;
        unlockRequested(unlockKeyInput.text);
        close();
    }

    title: qsTr("Unlock key backup")
    titleIcon: ":/icons/icons/ui/key.svg"
    titleIconColor: Komai.theme.success
    onOpened: {
        completionHandled = false;
        unlockKeyInput.text = "";
        unlockKeyInput.forceActiveFocus();
    }
    onClosed: {
        if (!completionHandled)
            cancelled();
    }

    Label {
        Layout.fillWidth: true
        color: palette.text
        text: qsTr("Unlock encryption secrets by providing your security key or its passphrase (if available).")
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
    }

    Label {
        Layout.fillWidth: true
        color: palette.text
        text: qsTr("Security keys look like this: ") + "<code>" + root.securityKeyExample + "</code>"
        textFormat: Text.RichText
        wrapMode: Text.Wrap
    }

    TextField {
        id: unlockKeyInput

        Layout.fillWidth: true
        echoMode: TextInput.Password
        placeholderText: qsTr("Security key or passphrase")

        onAccepted: root.submitUnlock()
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Komai.paddingSmall

        Components.KomaiButton {
            text: qsTr("Cancel")
            onClicked: root.close()
        }

        Item {
            Layout.fillWidth: true
        }

        Components.KomaiButton {
            icon.source: "qrc:/icons/icons/ui/key.svg"
            text: qsTr("Unlock")
            enabled: unlockKeyInput.text.length > 0
            highlighted: true

            onClicked: root.submitUnlock()
        }
    }
}
