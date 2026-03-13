// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components" as Components
import "../../ui"
import "./components"
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import cc.etke.komai 1.0

Components.OverlayDialog {
    id: inputDialog

    property alias prompt: promptLabel.text
    property alias echoMode: statusInput.echoMode

    signal phoneNumberAccepted(countryCode: string, text: string)

    titleIcon: ":/icons/icons/ui/send.svg"
    initialFocusItem: statusInput

    Label {
        id: promptLabel

        Layout.fillWidth: true
        color: palette.text
        wrapMode: Text.WordWrap
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Komai.paddingMedium

        Components.KomaiComboBox {
            id: numberPrefix

            editable: false

            delegate: ItemDelegate {
                text: n + " (" + p + ")"
            }

            //n=name,i=ISO,p=prefix -- see countries.js.md for source
            model: CountryDialCodesModel {
            }
        }

        MatrixTextField {
            id: statusInput

            Layout.fillWidth: true
            onAccepted: {
                inputDialog.phoneNumberAccepted(numberPrefix.model.get(numberPrefix.currentIndex).i, statusInput.text);
                inputDialog.close();
            }
        }
    }

    Components.KomaiButton {
        Layout.alignment: Qt.AlignRight
        text: qsTr("Continue")
        highlighted: true
        onClicked: {
            inputDialog.phoneNumberAccepted(numberPrefix.model.get(numberPrefix.currentIndex).i, statusInput.text);
            inputDialog.close();
        }
    }
}
