// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import ".."
import Qt.labs.platform 1.1 as P
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import im.nheko 1.0

Item {
    enabled: false
    visible: false
    SaveSecurityKeyDialog {
        id: showRecoverKeyDialog
    }

    UnlockKeyBackupDialog {
        id: unlockKeyBackupDialog

        onUnlockRequested: value => Nheko.submitUnlockKeyBackup(value)
        onCancelled: Nheko.cancelUnlockKeyBackup()
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

        onAccepted: SelfVerificationStatus.setupCrosssigning(storeSecretsOnline.checked, usePassword.checked ? passwordField.text : "", encryptionBackupOnlineEnabled.checked)

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
                    id: encryptionBackupOnlineEnabled

                    checked: true

                    onClicked: console.log("Online key backup toggled: " + checked)
                }
            }
        }
    }
    VerifyMasterKeyDialog {
        id: verifyMasterKey

        hasSSSS: SelfVerificationStatus.hasSSSS
        canVerifyWithAnotherDevice: SelfVerificationStatus.canVerifyWithAnotherDevice

        onVerifyWithAnotherDevice: {
            if (SelfVerificationStatus.verifyMasterKey())
                verifyMasterKey.close();
        }
        onUnlockKeyBackup: SelfVerificationStatus.verifyMasterKeyWithPassphrase()
        onResetIdentity: confirmResetIdentity.open()
    }

    ResetEncryptionIdentityDialog {
        id: confirmResetIdentity

        onResetRequested: {
            SelfVerificationStatus.resetEncryptionIdentity();
            verifyMasterKey.close();
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
                confirmResetIdentity.close();
                unlockKeyBackupDialog.close();
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
