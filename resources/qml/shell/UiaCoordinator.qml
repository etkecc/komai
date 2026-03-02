// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Dialogs
import cc.etke.komai

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
        titleIcon: ":/icons/icons/ui/key.svg"
        acceptText: qsTr("Continue")

        onInputAccepted: t => {
            return UIA.continuePassword(t);
        }
    }
    InputDialog {
        id: uiaEmailPrompt

        prompt: qsTr("Please enter a valid email address to continue:")
        title: UIA.title
        titleIcon: ":/icons/icons/ui/send.svg"
        acceptText: qsTr("Continue")

        onInputAccepted: t => {
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
        titleIcon: ":/icons/icons/ui/shield-regular.svg"
        acceptText: qsTr("Continue")

        onInputAccepted: t => {
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
            uiaEmailPrompt.open();
        }
        function onFallbackAuth(fallback) {
            root.showManagedDialog(componentCatalog.accountFallbackAuthDialog, {
                    "fallback": fallback
                });
        }
        function onPassword() {
            console.log("UIA: password needed");
            uiaPassPrompt.open();
        }
        function onPhoneNumber() {
            uiaPhoneNumberPrompt.open();
        }
        function onPrompt3pidToken() {
            uiaTokenPrompt.open();
        }
        function onReCaptcha(recaptcha) {
            root.showManagedDialog(componentCatalog.accountReCaptchaDialog, {
                    "recaptcha": recaptcha
                });
        }

        target: UIA
    }
}
