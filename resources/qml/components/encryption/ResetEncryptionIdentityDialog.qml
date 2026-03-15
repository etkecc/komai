// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import ".." as Components
import QtQuick 2.15
import QtQuick.Layouts 1.3
import cc.etke.komai 1.0

Components.OverlayDialog {
    id: root

    signal resetRequested()

    title: qsTr("Reset encryption identity?")
    titleIcon: ":/icons/icons/ui/shield-regular-exclamation-mark.svg"
    titleIconColor: Komai.theme.warning

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
        spacing: Komai.paddingSmall

        Components.KomaiButton {
            Layout.rightMargin: Komai.paddingLarge
            text: qsTr("Not now")
            onClicked: root.close()
        }

        Item {
            Layout.fillWidth: true
        }

        Components.KomaiButton {
            icon.source: "qrc:/icons/icons/ui/refresh.svg"
            text: qsTr("Reset")
            highlighted: true

            onClicked: {
                resetRequested();
                root.close();
            }
        }
    }
}
