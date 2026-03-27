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
    signal resetRequested(string password)
    signal cancelled()

    function submitReset()
    {
        if (passwordInput.text.length === 0)
            return;

        completionHandled = true;
        resetRequested(passwordInput.text);
        close();
    }

    title: qsTr("Confirm identity reset")
    titleIcon: ":/icons/icons/ui/refresh.svg"
    titleIconColor: Komai.theme.warning
    onOpened: {
        completionHandled = false;
        passwordInput.text = "";
        passwordInput.forceActiveFocus();
    }
    onClosed: {
        if (!completionHandled)
            cancelled();
    }

    Label {
        Layout.fillWidth: true
        color: palette.text
        text: qsTr("Your homeserver requires your account password before it will reset this device's encryption identity.")
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
    }

    Components.KomaiTextField {
        id: passwordInput

        Layout.fillWidth: true
        echoMode: TextInput.Password
        placeholderText: qsTr("Account password")

        onAccepted: root.submitReset()
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
            icon.source: "qrc:/icons/icons/ui/refresh.svg"
            text: qsTr("Reset")
            enabled: passwordInput.text.length > 0
            highlighted: true

            onClicked: root.submitReset()
        }
    }
}
