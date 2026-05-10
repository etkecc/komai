// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import ".." as Components
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai 1.0

Components.OverlayDialog {
    id: root

    property string recoveryKey: ""
    property bool copied: false

    closePolicy: Popup.NoAutoClose
    title: qsTr("Save your recovery key and keep it private")
    titleIcon: ":/icons/icons/ui/shield-regular-checkmark.svg"
    titleIconColor: Komai.theme.success
    onOpened: recoveryKeyField.forceActiveFocus()

    Label {
        Layout.fillWidth: true
        color: palette.text
        text: qsTr("Your encryption secrets are now stored on the server, encrypted using the key below.")
            + "\n\n"
            + qsTr("You'll need it to access encrypted messages if you sign out, reinstall, or set up another device.")
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Komai.paddingSmall

        Components.KomaiTextField {
            id: recoveryKeyField

            Layout.fillWidth: true
            readOnly: true
            font.bold: true
            text: root.recoveryKey
            KeyNavigation.tab: copyRecoveryKeyButton
        }

        Components.KomaiButton {
            id: copyRecoveryKeyButton

            activeFocusOnTab: true
            focusPolicy: Qt.StrongFocus
            KeyNavigation.tab: confirmRecoveryKeyButton
            KeyNavigation.backtab: recoveryKeyField
            icon.source: root.copied ? "qrc:/icons/icons/ui/checkmark.svg" : "qrc:/icons/icons/ui/copy.svg"
            text: root.copied ? qsTr("Copied") : qsTr("Copy")

            function activateCopy()
            {
                Clipboard.text = root.recoveryKey;
                root.copied = true;
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
                onTriggered: root.copied = false
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true

        Item {
            Layout.fillWidth: true
        }

        Components.KomaiButton {
            id: confirmRecoveryKeyButton

            icon.source: "qrc:/icons/icons/ui/checkmark.svg"
            KeyNavigation.backtab: copyRecoveryKeyButton
            text: qsTr("OK, I saved my recovery key")
            highlighted: true
            onClicked: root.close()
            Keys.onReturnPressed: event => {
                root.close();
                event.accepted = true;
            }
            Keys.onEnterPressed: event => {
                root.close();
                event.accepted = true;
            }
        }
    }
}
