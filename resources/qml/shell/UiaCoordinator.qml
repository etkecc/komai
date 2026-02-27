// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../dialogs/common"
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import im.nheko

Item {
    id: root
    required property var timelineRoot
    required property var componentCatalog

    function showManagedDialog(componentUrl, properties, onCreated) {
        var component = Qt.createComponent(componentUrl);
        if (component.status === Component.Ready) {
            var dialog = component.createObject(timelineRoot, properties || {});
            if (!dialog) {
                console.error("Failed to create dialog object for: " + componentUrl);
                return;
            }
            if (onCreated)
                onCreated(dialog);
            else
                dialog.show();
            timelineRoot.destroyOnClose(dialog);
        } else {
            console.error("Failed to create component: " + component.errorString());
        }
    }

    InputDialog {
        id: uiaPassPrompt

        echoMode: TextInput.Password
        prompt: qsTr("Please enter your login password to continue:")
        title: UIA.title

        onAccepted: t => {
            return UIA.continuePassword(t);
        }
    }
    InputDialog {
        id: uiaEmailPrompt

        prompt: qsTr("Please enter a valid email address to continue:")
        title: UIA.title

        onAccepted: t => {
            return UIA.continueEmail(t);
        }
    }
    PhoneNumberInputDialog {
        id: uiaPhoneNumberPrompt

        prompt: qsTr("Please enter a valid phone number to continue:")
        title: UIA.title

        onAccepted: (p, t) => {
            return UIA.continuePhoneNumber(p, t);
        }
    }
    InputDialog {
        id: uiaTokenPrompt

        prompt: qsTr("Please enter the token which has been sent to you:")
        title: UIA.title

        onAccepted: t => {
            return UIA.submit3pidToken(t);
        }
    }
    MessageDialog {
        id: uiaConfirmationLinkDialog

        buttons: MessageDialog.Ok
        text: qsTr("Wait for the confirmation link to arrive, then continue.")

        onAccepted: UIA.continue3pidReceived()
    }
    Connections {
        function onConfirm3pidToken() {
            uiaConfirmationLinkDialog.open();
        }
        function onEmail() {
            uiaEmailPrompt.show();
        }
        function onFallbackAuth(fallback) {
            root.showManagedDialog(componentCatalog.accountFallbackAuthDialog, {
                    "fallback": fallback
                });
        }
        function onPassword() {
            console.log("UIA: password needed");
            uiaPassPrompt.show();
        }
        function onPhoneNumber() {
            uiaPhoneNumberPrompt.show();
        }
        function onPrompt3pidToken() {
            uiaTokenPrompt.show();
        }
        function onReCaptcha(recaptcha) {
            root.showManagedDialog(componentCatalog.accountReCaptchaDialog, {
                    "recaptcha": recaptcha
                });
        }

        target: UIA
    }
}
