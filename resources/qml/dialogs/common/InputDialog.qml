// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components" as Components
import "../../ui"
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import cc.etke.komai 1.0

Components.OverlayDialog {
    id: inputDialog

    property alias prompt: promptLabel.text
    property alias echoMode: statusInput.echoMode
    property alias placeholderText: statusInput.placeholderText
    property alias text: statusInput.text
    property string acceptText: qsTr("OK")

    signal inputAccepted(text: string)

    initialFocusItem: statusInput

    Label {
        id: promptLabel

        Layout.fillWidth: true
        color: palette.text
        visible: text.length > 0
        wrapMode: Text.WordWrap
    }

    Components.KomaiTextField {
        id: statusInput

        Layout.fillWidth: true
        focus: true
        onAccepted: {
            inputDialog.inputAccepted(statusInput.text);
            inputDialog.close();
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Komai.paddingMedium

        Components.KomaiButton {
            text: qsTr("Cancel")
            onClicked: inputDialog.close()
        }

        Item {
            Layout.fillWidth: true
        }

        Components.KomaiButton {
            text: inputDialog.acceptText
            highlighted: true
            onClicked: {
                inputDialog.inputAccepted(statusInput.text);
                inputDialog.close();
            }
        }
    }
}
