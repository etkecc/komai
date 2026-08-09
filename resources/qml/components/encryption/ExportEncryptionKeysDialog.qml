// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import ".." as Components
import QtCore
import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs as Dialogs
import QtQuick.Layouts
import cc.etke.komai 1.0

Components.OverlayDialog {
    id: root

    // -1 while no export has completed; the exported key count afterwards.
    property int exportedCount: -1
    property string errorText: ""
    readonly property bool passphraseAcceptable: passphraseField.text.length > 0
        && passphraseField.text === confirmPassphraseField.text
    readonly property bool finished: exportedCount >= 0

    title: qsTr("Export encryption keys")
    titleIcon: ":/icons/icons/ui/key.svg"

    // e.g. "2026-08-09-1518-matrix-account-slavi-devture.com-e2ee-keys.txt"
    function suggestedKeyFileName() {
        const stamp = Qt.formatDateTime(new Date(), "yyyy-MM-dd-HHmm");
        const user = String(Settings.userId ?? "")
            .replace(/^@/, "")
            .replace(/[^A-Za-z0-9._-]/g, "-");
        return stamp + "-matrix-account-" + user + "-e2ee-keys.txt";
    }

    function openSaveDialog() {
        saveFileDialog.selectedFile = saveFileDialog.currentFolder + "/" + suggestedKeyFileName();
        saveFileDialog.open();
    }

    onOpened: {
        exportedCount = -1;
        errorText = "";
        passphraseField.text = "";
        confirmPassphraseField.text = "";
        passphraseField.forceActiveFocus();
    }

    Connections {
        function onExportCompleted(count) {
            root.exportedCount = count;
            root.errorText = "";
        }
        function onExportFailed(error) {
            root.errorText = error;
        }

        target: EncryptionKeyExport
    }

    Label {
        Layout.fillWidth: true
        color: palette.text
        text: qsTr("Saves the keys for reading your encrypted message history into a passphrase-protected file.")
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
    }

    Label {
        Layout.fillWidth: true
        color: Komai.theme.attention
        text: qsTr("Keep the file and its passphrase private, like your account credentials.")
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
    }

    Label {
        Layout.fillWidth: true
        color: palette.buttonText
        text: qsTr("Works with Komai, Element, Nheko and other Matrix clients.")
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
    }

    Components.KomaiTextField {
        id: passphraseField

        Layout.fillWidth: true
        echoMode: TextInput.Password
        placeholderText: qsTr("Passphrase")
        enabled: !EncryptionKeyExport.busy && !root.finished
    }

    Components.KomaiTextField {
        id: confirmPassphraseField

        Layout.fillWidth: true
        echoMode: TextInput.Password
        placeholderText: qsTr("Confirm passphrase")
        enabled: !EncryptionKeyExport.busy && !root.finished
        onAccepted: if (root.passphraseAcceptable) root.openSaveDialog()
    }

    // Single status line above the action row. Always present (an empty
    // Label still reserves one line of height), so routine validation
    // toggles don't shift the buttons.
    Label {
        Layout.fillWidth: true
        color: root.finished ? Komai.theme.success : Komai.theme.error
        text: {
            if (root.finished)
                return qsTr("Exported %n encryption key(s).", "", root.exportedCount);
            if (root.errorText.length > 0)
                return qsTr("Export failed: %1").arg(root.errorText);
            if (confirmPassphraseField.text.length > 0 && !root.passphraseAcceptable)
                return qsTr("Passphrases do not match.");
            return "";
        }
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Komai.paddingSmall

        Components.KomaiButton {
            visible: !root.finished
            text: qsTr("Cancel")
            enabled: !EncryptionKeyExport.busy
            onClicked: root.close()
        }

        Item {
            Layout.fillWidth: true
        }

        Components.KomaiButton {
            visible: !root.finished
            icon.source: "qrc:/icons/icons/ui/download.svg"
            text: qsTr("Export to file")
            enabled: root.passphraseAcceptable && !EncryptionKeyExport.busy
            highlighted: true
            onClicked: root.openSaveDialog()
        }

        Components.KomaiButton {
            visible: root.finished
            icon.source: "qrc:/icons/icons/ui/checkmark.svg"
            text: qsTr("Done")
            highlighted: true
            onClicked: root.close()
        }
    }

    Dialogs.FileDialog {
        id: saveFileDialog

        fileMode: Dialogs.FileDialog.SaveFile
        currentFolder: StandardPaths.writableLocation(StandardPaths.DocumentsLocation)
        defaultSuffix: "txt"
        nameFilters: [qsTr("Text files (*.txt)"), qsTr("All files (*)")]
        title: qsTr("Export encryption keys to file")
        onAccepted: EncryptionKeyExport.exportKeys(selectedFile, passphraseField.text)
    }
}
