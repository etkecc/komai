// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "./components/"
import Qt.labs.platform 1.1 as P
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import im.nheko 1.0

Item {
    id: root

    enabled: false
    visible: false
    readonly property int overlayDialogMinWidth: 520
    readonly property real overlayDialogMaxWidthRatio: 0.8

    function overlayDialogWidth(dialogParent, contentImplicitWidth, dialogPadding)
    {
        const parentWidth = dialogParent ? dialogParent.width : 760;
        const viewportMax = Math.max(240, parentWidth - Nheko.paddingLarge * 2);
        const ratioMax = Math.max(240, Math.floor(parentWidth * overlayDialogMaxWidthRatio));
        const maxWidth = Math.min(viewportMax, ratioMax);
        const minWidth = Math.min(overlayDialogMinWidth, maxWidth);
        const contentWidth = Math.ceil(contentImplicitWidth + dialogPadding * 2);
        return Math.max(minWidth, Math.min(maxWidth, contentWidth));
    }

    Dialog {
        id: showRecoverKeyDialog

        property string recoveryKey: ""
        property bool copied: false

        modal: true
        padding: Nheko.paddingMedium
        closePolicy: Popup.NoAutoClose
        standardButtons: Dialog.NoButton
        width: root.overlayDialogWidth(parent, contentItem ? contentItem.implicitWidth : 0, padding)
        x: Math.round(((parent ? parent.width : width) - width) / 2)
        y: Math.round((parent ? parent.height : 0) / 4)
        onOpened: recoveryKeyField.forceActiveFocus()

        // Workaround palettes not inheriting for popups
        palette: timelineRoot.palette
        parent: Overlay.overlay

        Overlay.modal: Rectangle {
            color: Qt.rgba(palette.window.r, palette.window.g, palette.window.b, 0.7)
        }

        background: Rectangle {
            color: palette.alternateBase
            radius: 8
        }

        contentItem: ColumnLayout {
            spacing: Nheko.paddingMedium

            RowLayout {
                Layout.fillWidth: true
                spacing: Nheko.paddingSmall

                Image {
                    Layout.alignment: Qt.AlignVCenter
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 24
                    fillMode: Image.PreserveAspectFit
                    source: "image://colorimage/:/icons/icons/ui/shield-regular-checkmark.svg?" + Nheko.theme.green
                    sourceSize.width: width * Screen.devicePixelRatio
                    sourceSize.height: height * Screen.devicePixelRatio
                }
                Label {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    color: palette.text
                    font.bold: true
                    font.pointSize: Settings.fontSize * 1.2
                    text: qsTr("Save your security key and keep it private")
                }
                ImageButton {
                    Layout.alignment: Qt.AlignVCenter
                    Layout.preferredWidth: 20
                    Layout.preferredHeight: 20
                    ToolTip.text: qsTr("Close")
                    ToolTip.visible: hovered
                    image: ":/icons/icons/ui/dismiss.svg"

                    onClicked: showRecoverKeyDialog.close()
                }
            }

            Label {
                Layout.fillWidth: true
                color: palette.text
                text: qsTr("Your encryption secrets are now stored on the server, encrypted using the key below.\n\nYou’ll need it to access encrypted messages if you sign out, reinstall, or set up another device.")
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Nheko.paddingSmall

                TextField {
                    id: recoveryKeyField

                    Layout.fillWidth: true
                    readOnly: true
                    font.bold: true
                    text: showRecoverKeyDialog.recoveryKey
                    KeyNavigation.tab: copyRecoveryKeyButton
                }

                Button {
                    id: copyRecoveryKeyButton

                    activeFocusOnTab: true
                    focusPolicy: Qt.StrongFocus
                    KeyNavigation.tab: confirmRecoveryKeyButton
                    KeyNavigation.backtab: recoveryKeyField
                    icon.source: showRecoverKeyDialog.copied
                                 ? "qrc:/icons/icons/ui/checkmark.svg"
                                 : "qrc:/icons/icons/ui/copy.svg"
                    icon.width: 16
                    icon.height: 16
                    text: showRecoverKeyDialog.copied ? qsTr("Copied") : qsTr("Copy")

                    function activateCopy()
                    {
                        Clipboard.text = showRecoverKeyDialog.recoveryKey;
                        showRecoverKeyDialog.copied = true;
                        copyRecoveryKeyFeedbackTimer.restart();
                    }
                    onClicked: activateCopy()
                    Keys.onReturnPressed: event => {
                        activateCopy();
                        event.accepted = true;
                    }
                    Keys.onEnterPressed: event => {
                        activateCopy();
                        event.accepted = true;
                    }

                    Timer {
                        id: copyRecoveryKeyFeedbackTimer

                        interval: 2000
                        onTriggered: showRecoverKeyDialog.copied = false
                    }
                }
            }

            RowLayout {
                Layout.fillWidth: true

                Item {
                    Layout.fillWidth: true
                }

                Button {
                    id: confirmRecoveryKeyButton

                    icon.source: "qrc:/icons/icons/ui/checkmark.svg"
                    icon.width: 18
                    icon.height: 18
                    KeyNavigation.backtab: copyRecoveryKeyButton
                    text: qsTr("OK, I saved my security key")
                    highlighted: true
                    onClicked: showRecoverKeyDialog.close()
                    Keys.onReturnPressed: event => {
                        showRecoverKeyDialog.close();
                        event.accepted = true;
                    }
                    Keys.onEnterPressed: event => {
                        showRecoverKeyDialog.close();
                        event.accepted = true;
                    }
                }
            }
        }
    }

    Dialog {
        id: unlockKeyBackupDialog

        property bool completionHandled: false
        readonly property string securityKeyExample: "EsUJ G5yL 75Sw 5cas scd8 4gSU rdVX Uuzk QsKC vYxe rTdc Hxee"
        function submitUnlock()
        {
            if (unlockKeyInput.text.length === 0)
                return;

            unlockKeyBackupDialog.completionHandled = true;
            Nheko.submitUnlockKeyBackup(unlockKeyInput.text);
            unlockKeyBackupDialog.close();
        }

        modal: true
        padding: Nheko.paddingMedium
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        standardButtons: Dialog.NoButton
        width: root.overlayDialogWidth(parent, contentItem ? contentItem.implicitWidth : 0, padding)
        x: Math.round(((parent ? parent.width : width) - width) / 2)
        y: Math.round((parent ? parent.height : 0) / 4)
        onOpened: {
            completionHandled = false;
            unlockKeyInput.text = "";
            unlockKeyInput.forceActiveFocus();
        }
        onClosed: {
            if (!completionHandled)
                Nheko.cancelUnlockKeyBackup();
        }

        // Workaround palettes not inheriting for popups
        palette: timelineRoot.palette
        parent: Overlay.overlay

        Overlay.modal: Rectangle {
            color: Qt.rgba(palette.window.r, palette.window.g, palette.window.b, 0.7)
        }

        background: Rectangle {
            color: palette.alternateBase
            radius: 8
        }

        contentItem: ColumnLayout {
            spacing: Nheko.paddingMedium

            RowLayout {
                Layout.fillWidth: true
                spacing: Nheko.paddingSmall

                Image {
                    Layout.alignment: Qt.AlignVCenter
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 24
                    fillMode: Image.PreserveAspectFit
                    source: "image://colorimage/:/icons/icons/ui/key.svg?" + Nheko.theme.green
                    sourceSize.width: width * Screen.devicePixelRatio
                    sourceSize.height: height * Screen.devicePixelRatio
                }
                Label {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    color: palette.text
                    font.bold: true
                    font.pointSize: Settings.fontSize * 1.2
                    text: qsTr("Unlock key backup")
                }
                ImageButton {
                    Layout.alignment: Qt.AlignVCenter
                    Layout.preferredWidth: 20
                    Layout.preferredHeight: 20
                    ToolTip.text: qsTr("Close")
                    ToolTip.visible: hovered
                    image: ":/icons/icons/ui/dismiss.svg"

                    onClicked: unlockKeyBackupDialog.close()
                }
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
                text: qsTr("Security keys look like this: ") + "<code>" + unlockKeyBackupDialog.securityKeyExample + "</code>"
                textFormat: Text.RichText
                wrapMode: Text.Wrap
            }

            TextField {
                id: unlockKeyInput

                Layout.fillWidth: true
                echoMode: TextInput.Password
                placeholderText: qsTr("Security key or passphrase")

                onAccepted: unlockKeyBackupDialog.submitUnlock()
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Nheko.paddingSmall

                Button {
                    text: qsTr("Cancel")
                    onClicked: unlockKeyBackupDialog.close()
                }

                Item {
                    Layout.fillWidth: true
                }

                Button {
                    id: unlockKeyButton

                    icon.source: "qrc:/icons/icons/ui/key.svg"
                    icon.width: 18
                    icon.height: 18
                    text: qsTr("Unlock")
                    enabled: unlockKeyInput.text.length > 0
                    highlighted: true

                    onClicked: unlockKeyBackupDialog.submitUnlock()
                }
            }
        }
    }

    P.MessageDialog {
        id: successDialog

        buttons: P.MessageDialog.Ok
        text: qsTr("Encryption setup successfully")
    }
    P.MessageDialog {
        id: failureDialog

        property string errorMessage

        buttons: P.MessageDialog.Ok
        text: qsTr("Failed to setup encryption: %1").arg(errorMessage)
    }
    MainWindowDialog {
        id: bootstrapCrosssigning

        // Workaround palettes not inheriting for popups
        palette: timelineRoot.palette

        background: Rectangle {
            border.color: Nheko.theme.separator
            border.width: 1
            color: palette.window
            radius: Nheko.paddingSmall
        }

        onAccepted: SelfVerificationStatus.setupCrosssigning(storeSecretsOnline.checked, usePassword.checked ? passwordField.text : "", onlineKeyBackupEnabled.checked)

        GridLayout {
            id: grid

            columnSpacing: 0
            columns: 2
            rowSpacing: 0
            width: bootstrapCrosssigning.useableWidth
            z: 1

            Label {
                Layout.alignment: Qt.AlignHCenter
                Layout.columnSpan: 2
                Layout.margins: Nheko.paddingMedium
                color: palette.text
                font.pointSize: Settings.fontSize * 2
                text: qsTr("Setup Encryption")
                wrapMode: Text.Wrap
            }
            Label {
                Layout.alignment: Qt.AlignLeft
                Layout.columnSpan: 2
                Layout.margins: Nheko.paddingMedium
                Layout.maximumWidth: grid.width - Nheko.paddingMedium * 2
                color: palette.text
                text: qsTr("Hello and welcome to Matrix!\nIt seems like you are new. Before you can securely encrypt your messages, we need to setup a few small things. You can either press accept immediately or adjust a few basic options. We also try to explain a few of the basics. You can skip those parts, but they might prove to be helpful!")
                wrapMode: Text.Wrap
            }
            Label {
                Layout.alignment: Qt.AlignLeft
                Layout.columnSpan: 1
                Layout.margins: Nheko.paddingMedium
                Layout.maximumWidth: Math.floor(grid.width / 2) - Nheko.paddingMedium * 2
                color: palette.text
                text: "Store secrets online.\nYou have a few secrets to make all the encryption magic work. While you can keep them stored only locally, we recommend storing them encrypted on the server. Otherwise it will be painful to recover them. Only disable this if you are paranoid and like losing your data!"
                wrapMode: Text.Wrap
            }
            Item {
                Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
                Layout.fillWidth: true
                Layout.margins: Nheko.paddingMedium
                Layout.preferredHeight: storeSecretsOnline.height

                ToggleButton {
                    id: storeSecretsOnline

                    checked: true

                    onClicked: console.log("Store secrets toggled: " + checked)
                }
            }
            Label {
                Layout.alignment: Qt.AlignLeft
                Layout.columnSpan: 1
                Layout.margins: Nheko.paddingMedium
                Layout.maximumWidth: Math.floor(grid.width / 2) - Nheko.paddingMedium * 2
                Layout.rowSpan: 2
                color: palette.text
                text: "Set an online backup password.\nWe recommend you DON'T set a password and instead only rely on the recovery key. You will get a recovery key in any case when storing the cross-signing secrets online, but passwords are usually not very random, so they are easier to attack than a completely random recovery key. If you choose to use a password, DON'T make it the same as your login password, otherwise your server can read all your encrypted messages. (You don't want that.)"
                visible: storeSecretsOnline.checked
                wrapMode: Text.Wrap
            }
            Item {
                Layout.alignment: Qt.AlignLeft | Qt.AlignTop
                Layout.fillWidth: true
                Layout.margins: Nheko.paddingMedium
                Layout.preferredHeight: storeSecretsOnline.height
                Layout.rowSpan: usePassword.checked ? 1 : 2
                Layout.topMargin: Nheko.paddingLarge
                visible: storeSecretsOnline.checked

                ToggleButton {
                    id: usePassword

                    checked: false
                }
            }
            MatrixTextField {
                id: passwordField

                Layout.alignment: Qt.AlignLeft | Qt.AlignTop
                Layout.columnSpan: 1
                Layout.fillWidth: true
                Layout.margins: Nheko.paddingMedium
                Layout.maximumWidth: Math.floor(grid.width / 2) - Nheko.paddingMedium * 2
                echoMode: TextInput.Password
                visible: storeSecretsOnline.checked && usePassword.checked
            }
            Label {
                Layout.alignment: Qt.AlignLeft
                Layout.columnSpan: 1
                Layout.margins: Nheko.paddingMedium
                Layout.maximumWidth: Math.floor(grid.width / 2) - Nheko.paddingMedium * 2
                color: palette.text
                text: "Use online key backup.\nStore the keys for your messages securely encrypted online. In general you do want this, because it protects your messages from becoming unreadable, if you log out by accident. It does however carry a small security risk, if you ever share your recovery key by accident. Currently this also has some other weaknesses, that might allow the server to insert new keys into your backup. The server will however never be able to read your messages."
                wrapMode: Text.Wrap
            }
            Item {
                Layout.alignment: Qt.AlignLeft | Qt.AlignVCenter
                Layout.fillWidth: true
                Layout.margins: Nheko.paddingMedium
                Layout.preferredHeight: storeSecretsOnline.height

                ToggleButton {
                    id: onlineKeyBackupEnabled

                    checked: true

                    onClicked: console.log("Online key backup toggled: " + checked)
                }
            }
        }
    }
    Dialog {
        id: verifyMasterKey

        // Workaround palettes not inheriting for popups
        palette: timelineRoot.palette

        parent: Overlay.overlay
        modal: true
        padding: Nheko.paddingMedium
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        standardButtons: Dialog.NoButton
        width: root.overlayDialogWidth(parent, contentItem ? contentItem.implicitWidth : 0, padding)
        x: Math.round(((parent ? parent.width : width) - width) / 2)
        y: Math.round((parent ? parent.height : 0) / 4)

        Overlay.modal: Rectangle {
            color: Qt.rgba(palette.window.r, palette.window.g, palette.window.b, 0.7)
        }

        background: Rectangle {
            color: palette.alternateBase
            radius: 8
        }

        contentItem: ColumnLayout {
            spacing: Nheko.paddingMedium

            RowLayout {
                Layout.fillWidth: true
                spacing: Nheko.paddingSmall

                Image {
                    Layout.alignment: Qt.AlignVCenter
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 24
                    fillMode: Image.PreserveAspectFit
                    source: "image://colorimage/:/icons/icons/ui/shield-regular-exclamation-mark.svg?" + palette.text
                    sourceSize.width: width * Screen.devicePixelRatio
                    sourceSize.height: height * Screen.devicePixelRatio
                }
                Label {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    color: palette.text
                    font.bold: true
                    font.pointSize: Settings.fontSize * 1.2
                    text: qsTr("Activate Encryption")
                }
                ImageButton {
                    Layout.alignment: Qt.AlignVCenter
                    Layout.preferredWidth: 20
                    Layout.preferredHeight: 20
                    ToolTip.text: qsTr("Close")
                    ToolTip.visible: hovered
                    image: ":/icons/icons/ui/dismiss.svg"

                    onClicked: verifyMasterKey.close()
                }
            }

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
                    onClicked: verifyMasterKey.close()
                }

                Item {
                    Layout.fillWidth: true
                }

                Button {
                    icon.source: "qrc:/icons/icons/ui/refresh.svg"
                    icon.width: 18
                    icon.height: 18
                    text: qsTr("Reset identity")

                    onClicked: {
                        confirmResetIdentity.open();
                    }
                }
                Button {
                    icon.source: "qrc:/icons/icons/ui/shield-regular-checkmark.svg"
                    icon.width: 18
                    icon.height: 18
                    text: qsTr("Verify with another device")

                    onClicked: {
                        SelfVerificationStatus.verifyMasterKey();
                        verifyMasterKey.close();
                    }
                }
                Button {
                    icon.source: "qrc:/icons/icons/ui/key.svg"
                    icon.width: 18
                    icon.height: 18
                    text: qsTr("Unlock key backup")
                    visible: SelfVerificationStatus.hasSSSS

                    onClicked: {
                        SelfVerificationStatus.verifyMasterKeyWithPassphrase();
                        verifyMasterKey.close();
                    }
                }
            }
        }
    }
    Dialog {
        id: confirmResetIdentity

        // Workaround palettes not inheriting for popups
        palette: timelineRoot.palette

        parent: Overlay.overlay
        modal: true
        padding: Nheko.paddingMedium
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        standardButtons: Dialog.NoButton
        width: root.overlayDialogWidth(parent, contentItem ? contentItem.implicitWidth : 0, padding)
        x: Math.round(((parent ? parent.width : width) - width) / 2)
        y: Math.round((parent ? parent.height : 0) / 4)

        Overlay.modal: Rectangle {
            color: Qt.rgba(palette.window.r, palette.window.g, palette.window.b, 0.7)
        }

        background: Rectangle {
            color: palette.alternateBase
            radius: 8
        }

        contentItem: ColumnLayout {
            spacing: Nheko.paddingMedium

            RowLayout {
                Layout.fillWidth: true
                spacing: Nheko.paddingSmall

                Image {
                    Layout.alignment: Qt.AlignVCenter
                    Layout.preferredWidth: 24
                    Layout.preferredHeight: 24
                    fillMode: Image.PreserveAspectFit
                    source: "image://colorimage/:/icons/icons/ui/shield-regular-exclamation-mark.svg?" + Nheko.theme.orange
                    sourceSize.width: width * Screen.devicePixelRatio
                    sourceSize.height: height * Screen.devicePixelRatio
                }
                Label {
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter
                    color: palette.text
                    font.bold: true
                    font.pointSize: Settings.fontSize * 1.2
                    text: qsTr("Reset encryption identity?")
                }
                ImageButton {
                    Layout.alignment: Qt.AlignVCenter
                    Layout.preferredWidth: 20
                    Layout.preferredHeight: 20
                    ToolTip.text: qsTr("Close")
                    ToolTip.visible: hovered
                    image: ":/icons/icons/ui/dismiss.svg"

                    onClicked: confirmResetIdentity.close()
                }
            }

            TextEdit {
                Layout.fillWidth: true
                color: palette.text
                readOnly: true
                selectByMouse: true
                text: qsTr("Resetting creates a new encryption identity for this account and starts setup again.\n\nYou will get a new security key. Better save it to avoid resetting again.\n\nPrevious server-side key backups are not removed automatically.")
                textFormat: TextEdit.PlainText
                wrapMode: TextEdit.Wrap
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Nheko.paddingSmall

                Button {
                    Layout.rightMargin: Nheko.paddingLarge
                    text: qsTr("Not now")
                    onClicked: confirmResetIdentity.close()
                }

                Item {
                    Layout.fillWidth: true
                }

                Button {
                    icon.source: "qrc:/icons/icons/ui/refresh.svg"
                    icon.width: 18
                    icon.height: 18
                    text: qsTr("Reset")
                    highlighted: true

                    onClicked: {
                        SelfVerificationStatus.resetEncryptionIdentity();
                        confirmResetIdentity.close();
                        verifyMasterKey.close();
                    }
                }
            }
        }
    }
    Connections {
        function onPromptUnlockKeyBackup() {
            unlockKeyBackupDialog.open();
        }

        target: Nheko
    }
    Connections {
        function onSetupCompleted() {
            successDialog.open();
        }
        function onSetupFailed(m) {
            failureDialog.errorMessage = m;
            failureDialog.open();
        }
        function onShowRecoveryKey(key) {
            showRecoverKeyDialog.recoveryKey = key;
            showRecoverKeyDialog.open();
        }
        function onStatusChanged() {
            if (SelfVerificationStatus.status == SelfVerificationStatus.AllVerified) {
                bootstrapCrosssigning.close();
                verifyMasterKey.close();
            }
        }
        function onPromptForStatus(status) {
            if (status === SelfVerificationStatus.NoMasterKey) {
                bootstrapCrosssigning.open();
            } else if (status === SelfVerificationStatus.UnverifiedMasterKey) {
                verifyMasterKey.open();
            }
        }

        target: SelfVerificationStatus
    }
}
