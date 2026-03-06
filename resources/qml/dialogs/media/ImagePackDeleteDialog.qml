// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components" as Components
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
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

        Button {
            text: qsTr("Cancel")
            onClicked: deleteStickerPackRoot.close()
        }

        Item {
            Layout.fillWidth: true
        }

        Button {
            text: qsTr("Delete")
            highlighted: true
            onClicked: {
                imagePack.remove();
                deleteStickerPackRoot.close();
            }
        }
    }
}
