// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../ui"
import "./components"
import QtQuick 2.12
import QtQuick.Controls 2.5
import QtQuick.Layouts 1.3
import im.nheko 1.0

ApplicationWindow {
    id: inputDialog

    property alias prompt: promptLabel.text
    property alias echoMode: statusInput.echoMode
    signal accepted(countryCode: string, text: string)

    modality: Qt.NonModal
    flags: Qt.Dialog
    width: 350
    height: fontMetrics.lineSpacing * 7

    GridLayout {
        rowSpacing: Nheko.paddingMedium
        columnSpacing: Nheko.paddingMedium
        anchors.margins: Nheko.paddingMedium
        anchors.fill: parent
        columns: 2

        Label {
            id: promptLabel

            Layout.columnSpan: 2
            color: palette.text
        }

        ComboBox {
            id: numberPrefix

            editable: false

            delegate: ItemDelegate {
                text: n + " (" + p + ")"
            }
            // taken from https://gitlab.com/whisperfish/whisperfish/-/blob/master/qml/js/countries.js

            //n=name,i=ISO,p=prefix -- see countries.js.md for source
            model: CountryDialCodesModel {
            }

        }

        MatrixTextField {
            id: statusInput

            Layout.fillWidth: true
        }

    }

    footer: DialogButtonBox {
        standardButtons: DialogButtonBox.Ok | DialogButtonBox.Cancel
        onAccepted: {
            inputDialog.accepted(numberPrefix.model.get(numberPrefix.currentIndex).i, statusInput.text);

            inputDialog.close();
        }
        onRejected: {
            inputDialog.close();
        }
    }

}
