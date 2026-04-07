// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components" as Components
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai 1.0

Components.OverlayDialog {
    id: deleteStickerPackRoot

    property SingleImagePackModel imagePack

    title: qsTr("Delete sticker pack")
    titleIcon: ":/icons/icons/ui/delete.svg"

    Label {
        Layout.fillWidth: true
        color: palette.text
        wrapMode: Text.WordWrap
        text: qsTr("Are you sure you wish to delete the sticker pack '%1'?").arg(imagePack.packname)
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Komai.paddingMedium

        Components.KomaiButton {
            text: qsTr("Cancel")
            onClicked: deleteStickerPackRoot.close()
        }

        Item {
            Layout.fillWidth: true
        }

        Components.KomaiButton {
            text: qsTr("Delete")
            highlighted: true
            onClicked: {
                imagePack.remove();
                deleteStickerPackRoot.close();
            }
        }
    }
}
