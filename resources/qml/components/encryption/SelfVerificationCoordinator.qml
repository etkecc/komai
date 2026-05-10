// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import ".." as Components
import "../../ui"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai 1.0

Item {
    enabled: false
    visible: false

    component OptionBadge: Rectangle {
        id: optionBadge

        required property string label
        required property color tone

        implicitWidth: optionBadgeLabel.implicitWidth + Komai.paddingSmall * 2
        implicitHeight: optionBadgeLabel.implicitHeight + Komai.paddingSmall
        radius: Komai.paddingSmall
        color: Qt.rgba(tone.r, tone.g, tone.b, 0.15)
        border.color: tone
        border.width: 1

        Label {
            id: optionBadgeLabel
            anchors.centerIn: parent
            text: optionBadge.label
            color: optionBadge.tone
            font.pointSize: Math.floor(Settings.uiFontSizePt * 0.85)
        }
    }

    SaveRecoveryKeyDialog {
        id: showRecoverKeyDialog
    }

    UnlockKeyBackupDialog {
        id: unlockKeyBackupDialog

        onUnlockRequested: value => SelfVerificationStatus.submitUnlockKeyBackup(value)
        onCancelled: SelfVerificationStatus.cancelUnlockKeyBackup()
    }

    ResetEncryptionIdentityPasswordDialog {
        id: resetEncryptionIdentityPasswordDialog

        onResetRequested: value => SelfVerificationStatus.submitResetEncryptionIdentityPassword(value)
        onCancelled: SelfVerificationStatus.cancelResetEncryptionIdentity()
    }

    ResetEncryptionIdentityApprovalDialog {
        id: resetEncryptionIdentityApprovalDialog

        onContinueRequested: SelfVerificationStatus.continueResetEncryptionIdentityAfterApproval()
        onCancelled: SelfVerificationStatus.cancelResetEncryptionIdentity()
    }

    Components.OverlayDialog {
        id: successDialog

        title: qsTr("Encryption setup complete")
        titleIcon: ":/icons/icons/ui/shield-regular-checkmark.svg"
        titleIconColor: Komai.theme.success

        Label {
            Layout.fillWidth: true
            color: palette.text
            wrapMode: Text.WordWrap
            text: qsTr("Encryption setup successfully")
        }
    }

    Components.OverlayDialog {
        id: unlockSuccessDialog

        title: qsTr("Encryption secrets unlocked")
        titleIcon: ":/icons/icons/ui/key.svg"
        titleIconColor: Komai.theme.success

        Label {
            Layout.fillWidth: true
            color: palette.text
            wrapMode: Text.WordWrap
            text: qsTr("This device can now use the recovered encryption secrets.")
        }
    }

    Components.OverlayDialog {
        id: resetSuccessDialog

        title: qsTr("Encryption identity reset")
        titleIcon: ":/icons/icons/ui/refresh.svg"
        titleIconColor: Komai.theme.success

        Label {
            Layout.fillWidth: true
            color: palette.text
            wrapMode: Text.WordWrap
            text: qsTr("A new encryption identity was created for this device. You may still want to set up backups again.")
        }
    }

    Components.OverlayDialog {
        id: failureDialog

        property string errorMessage

        title: qsTr("Encryption setup failed")
        titleIcon: ":/icons/icons/ui/shield-regular-exclamation-mark.svg"
        titleIconColor: Komai.theme.error

        TextEdit {
            Layout.fillWidth: true
            color: palette.text
            readOnly: true
            selectByMouse: true
            wrapMode: Text.WordWrap
            text: qsTr("Failed to setup encryption: %1").arg(failureDialog.errorMessage)
        }
    }
    Components.OverlayDialog {
        id: bootstrapCrosssigning

        title: qsTr("Set up encryption")
        titleIcon: ":/icons/icons/ui/shield-regular.svg"
        titleIconColor: palette.text
        closePolicy: Popup.NoAutoClose

        Label {
            Layout.fillWidth: true
            color: palette.text
            text: qsTr("End-to-end encryption keeps your messages private. Only you and the people you chat with can read them.")
            wrapMode: Text.Wrap
        }

        Label {
            Layout.fillWidth: true
            color: palette.text
            text: qsTr("For encryption to keep working across sign-ins or reinstalls, your encryption keys need to be preserved.")
            wrapMode: Text.Wrap
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: Komai.paddingSmall
            spacing: Komai.paddingMedium

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Komai.paddingSmall

                    Label {
                        color: palette.text
                        font.bold: true
                        text: qsTr("Save encryption keys to Secret Storage (SSSS)")
                        wrapMode: Text.Wrap
                    }
                    OptionBadge {
                        Layout.alignment: Qt.AlignVCenter
                        label: qsTr("Recommended")
                        tone: Komai.theme.success
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                }
                Label {
                    Layout.fillWidth: true
                    color: palette.text
                    opacity: 0.75
                    text: qsTr("Stores your cross-signing keys encrypted on the server, so new sign-ins can recover your encrypted identity.")
                    wrapMode: Text.Wrap
                }
            }

            ToggleButton {
                id: storeSecretsOnline

                checked: true
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Komai.paddingMedium
            opacity: storeSecretsOnline.checked ? 1.0 : 0.5

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Komai.paddingSmall

                    Label {
                        color: palette.text
                        font.bold: true
                        text: qsTr("Set up server-side key backup")
                        wrapMode: Text.Wrap
                    }
                    OptionBadge {
                        Layout.alignment: Qt.AlignVCenter
                        label: qsTr("Recommended")
                        tone: Komai.theme.success
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                }
                Label {
                    Layout.fillWidth: true
                    color: palette.text
                    opacity: 0.75
                    text: qsTr("Stores your message-decryption keys encrypted on the server, so older messages stay readable on new sign-ins.")
                    wrapMode: Text.Wrap
                }
            }

            ToggleButton {
                id: encryptionBackupOnlineEnabled

                enabled: storeSecretsOnline.checked
                checked: true
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: Komai.paddingMedium
            opacity: storeSecretsOnline.checked ? 1.0 : 0.5

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Komai.paddingSmall

                    Label {
                        color: palette.text
                        font.bold: true
                        text: qsTr("Allow unlocking Secret Storage with a recovery passphrase")
                        wrapMode: Text.Wrap
                    }
                    OptionBadge {
                        Layout.alignment: Qt.AlignVCenter
                        label: qsTr("Optional")
                        tone: palette.buttonText
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                }
                Label {
                    Layout.fillWidth: true
                    color: palette.text
                    opacity: 0.75
                    text: qsTr("A memorable phrase that unlocks the same Secret Storage as the recovery key.")
                    wrapMode: Text.Wrap
                }
            }

            ToggleButton {
                id: usePassword

                enabled: storeSecretsOnline.checked
                checked: false
                onCheckedChanged: if (checked) passwordField.forceActiveFocus()
            }
        }

        Components.KomaiTextField {
            id: passwordField

            Layout.fillWidth: true
            echoMode: TextInput.Password
            placeholderText: qsTr("Recovery passphrase")
            visible: storeSecretsOnline.checked && usePassword.checked
        }

        Label {
            Layout.fillWidth: true
            color: palette.text
            opacity: 0.65
            text: qsTr("For best security, don't reuse your account password.")
            wrapMode: Text.Wrap
            visible: storeSecretsOnline.checked && usePassword.checked
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: Komai.paddingSmall
            spacing: Komai.paddingSmall

            Components.KomaiButton {
                text: qsTr("Not now")
                onClicked: bootstrapCrosssigning.close()
            }

            Item {
                Layout.fillWidth: true
            }

            Components.KomaiButton {
                icon.source: "qrc:/icons/icons/ui/shield-regular-checkmark.svg"
                text: qsTr("Set up encryption")
                highlighted: true

                onClicked: {
                    SelfVerificationStatus.setupCrosssigning(storeSecretsOnline.checked, usePassword.checked ? passwordField.text : "", storeSecretsOnline.checked && encryptionBackupOnlineEnabled.checked);
                    bootstrapCrosssigning.close();
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

        function onSetupCompleted() {
            successDialog.open();
        }
        function onSetupFailed(m) {
            failureDialog.errorMessage = m;
            failureDialog.open();
        }
        function onUnlockKeyBackupCompleted() {
            unlockSuccessDialog.open();
        }
        function onResetEncryptionIdentityCompleted() {
            resetSuccessDialog.open();
        }
        function onPromptResetEncryptionIdentityPassword() {
            resetEncryptionIdentityPasswordDialog.open();
        }
        function onPromptResetEncryptionIdentityApproval(approvalUrl) {
            resetEncryptionIdentityApprovalDialog.approvalUrl = approvalUrl;
            resetEncryptionIdentityApprovalDialog.open();
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
    Connections {
        function onPromptUnlockKeyBackup() {
            unlockKeyBackupDialog.open();
        }

        target: Komai
    }
}
