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
    id: recaptchaRoot

    required property ReCaptcha recaptcha

    property bool wasAccepted: false

    title: recaptcha.context
    titleIcon: ":/icons/icons/ui/shield-regular.svg"

    onOpened: wasAccepted = false
    onClosed: {
        if (!wasAccepted)
            recaptcha.cancel();
    }

    Label {
        Layout.fillWidth: true
        color: palette.text
        wrapMode: Text.WordWrap
        text: qsTr("Solve the reCAPTCHA and press the confirm button")
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Komai.paddingMedium

        Button {
            text: qsTr("Cancel")
            onClicked: recaptchaRoot.close()
        }

        Button {
            text: qsTr("Open reCAPTCHA")
            onClicked: recaptcha.openReCaptcha()
        }

        Item { Layout.fillWidth: true }

        Button {
            text: qsTr("Confirm")
            highlighted: true
            onClicked: {
                recaptchaRoot.wasAccepted = true;
                recaptcha.confirm();
                recaptchaRoot.close();
            }
        }
    }
}
