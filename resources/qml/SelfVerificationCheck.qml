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
    enabled: false
    visible: false

    Dialog {
        id: showRecoverKeyDialog

        property string recoveryKey: ""

        anchors.centerIn: parent
        closePolicy: Popup.NoAutoClose
        height: content.height + implicitFooterHeight + implicitHeaderHeight
        modal: true
        padding: 0

        // Workaround palettes not inheriting for popups
        palette: timelineRoot.palette
        parent: Overlay.overlay
        standardButtons: Dialog.Ok
        width: content.width

        background: Rectangle {
            border.color: Nheko.theme.separator
            border.width: 1
            color: palette.alternateBase
            radius: 8
        }

        ColumnLayout {
            id: content

            spacing: 0

            Label {
                Layout.fillWidth: true
                Layout.margins: Nheko.paddingMedium
                Layout.maximumWidth: (showRecoverKeyDialog.Overlay.overlay ? showRecoverKeyDialog.Overlay.overlay.width : 400) - Nheko.paddingMedium * 4
                color: palette.text
                text: qsTr("This is your recovery key. You will need it to restore access to your encrypted messages and verification keys. Keep this safe. Don't share it with anyone and don't lose it! Do not pass go! Do not collect $200!")
                wrapMode: Text.Wrap
            }
            TextEdit {
                Layout.alignment: Qt.AlignHCenter
                Layout.maximumWidth: (showRecoverKeyDialog.Overlay.overlay ? showRecoverKeyDialog.Overlay.overlay.width : 400) - Nheko.paddingMedium * 4
                color: palette.text
                font.bold: true
                horizontalAlignment: TextEdit.AlignHCenter
                readOnly: true
                selectByMouse: true
                text: showRecoverKeyDialog.recoveryKey
                verticalAlignment: TextEdit.AlignVCenter
                wrapMode: TextEdit.Wrap
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
        width: Math.min(760, (parent ? parent.width : 760) - Nheko.paddingLarge * 2)
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
                text: qsTr("Encryption is already configured for this account. Verify this device to access encrypted messages and mark it as trusted. You can cancel and do this later.")
                textFormat: TextEdit.PlainText
                wrapMode: TextEdit.Wrap
            }

            RowLayout {
                Layout.fillWidth: true
                spacing: Nheko.paddingSmall

                Button {
                    text: qsTr("Cancel")
                    onClicked: verifyMasterKey.close()
                }

                Item {
                    Layout.fillWidth: true
                }

                Button {
                    text: qsTr("Enter passphrase")
                    visible: SelfVerificationStatus.hasSSSS

                    onClicked: {
                        SelfVerificationStatus.verifyMasterKeyWithPassphrase();
                        verifyMasterKey.close();
                    }
                }
                Button {
                    text: qsTr("Verify with another device")
                    highlighted: true

                    onClicked: {
                        SelfVerificationStatus.verifyMasterKey();
                        verifyMasterKey.close();
                    }
                }
            }
        }
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
            console.log("STATUS CHANGED: " + SelfVerificationStatus.status);
            if (SelfVerificationStatus.status == SelfVerificationStatus.NoMasterKey) {
                bootstrapCrosssigning.open();
            } else if (SelfVerificationStatus.status == SelfVerificationStatus.UnverifiedMasterKey) {
                verifyMasterKey.open();
            } else {
                bootstrapCrosssigning.close();
                verifyMasterKey.close();
            }
        }

        target: SelfVerificationStatus
    }
}
