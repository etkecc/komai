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

    property url keyFile: ""
    // -1 while no import has completed; the imported key count afterwards.
    property int importedCount: -1
    property int totalCount: 0
    property string errorText: ""
    readonly property bool finished: importedCount >= 0
    readonly property bool canImport: keyFile.toString().length > 0
        && passphraseField.text.length > 0
        && !EncryptionKeyExport.busy
        && !finished

    title: qsTr("Import encryption keys")
    titleIcon: ":/icons/icons/ui/key.svg"

    onOpened: {
        keyFile = "";
        importedCount = -1;
        totalCount = 0;
        errorText = "";
        passphraseField.text = "";
        openFileDialog.open();
    }

    Connections {
        function onImportCompleted(imported, total) {
            root.importedCount = imported;
            root.totalCount = total;
            root.errorText = "";
        }
        function onImportFailed(error) {
            root.errorText = error;
        }

        target: EncryptionKeyExport
    }

    Label {
        Layout.fillWidth: true
        color: palette.text
        text: qsTr("Loads keys from a previously exported key file, so this device can decrypt older encrypted messages. You'll need the passphrase the file was exported with.")
        textFormat: Text.PlainText
        wrapMode: Text.Wrap
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Komai.paddingSmall

        Components.KomaiTextField {
            id: fileField

            Layout.fillWidth: true
            readOnly: true
            placeholderText: qsTr("No file selected")
            text: {
                const path = root.keyFile.toString();
                return path.length > 0 ? decodeURIComponent(path.split("/").pop()) : "";
            }
        }

        Components.KomaiButton {
            text: qsTr("Choose file")
            enabled: !EncryptionKeyExport.busy && !root.finished
            onClicked: openFileDialog.open()
        }
    }

    Components.KomaiTextField {
        id: passphraseField

        Layout.fillWidth: true
        echoMode: TextInput.Password
        placeholderText: qsTr("Passphrase")
        enabled: !EncryptionKeyExport.busy && !root.finished
        onAccepted: if (root.canImport) importButton.clicked()
    }

    // Single status line above the action row. Always present (an empty
    // Label still reserves one line of height), so routine validation
    // toggles don't shift the buttons.
    Label {
        Layout.fillWidth: true
        color: root.finished ? Komai.theme.success : Komai.theme.error
        text: {
            if (root.finished) {
                let message = qsTr("Imported %1 of %2 keys.").arg(root.importedCount).arg(root.totalCount);
                if (root.importedCount < root.totalCount)
                    message += "\n" + qsTr("Keys this device already has are skipped.");
                return message;
            }
            if (root.errorText.length > 0)
                return qsTr("Import failed: %1").arg(root.errorText);
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
            id: importButton

            visible: !root.finished
            icon.source: "qrc:/icons/icons/ui/upload.svg"
            text: qsTr("Import")
            enabled: root.canImport
            highlighted: true
            onClicked: EncryptionKeyExport.importKeys(root.keyFile, passphraseField.text)
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
        id: openFileDialog

        fileMode: Dialogs.FileDialog.OpenFile
        currentFolder: StandardPaths.writableLocation(StandardPaths.DocumentsLocation)
        nameFilters: [qsTr("Text files (*.txt)"), qsTr("All files (*)")]
        title: qsTr("Select encryption key file")
        onAccepted: {
            root.keyFile = openFileDialog.selectedFile;
            passphraseField.forceActiveFocus();
        }
    }
}
