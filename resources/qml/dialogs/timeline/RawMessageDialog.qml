// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import cc.etke.komai

ApplicationWindow {
    id: rawMessageRoot

    property alias rawMessage: rawMessageView.text

    height: 420
    width: 420
    color: palette.window
    flags: Qt.Tool | Qt.WindowStaysOnTopHint | Qt.WindowCloseButtonHint | Qt.WindowTitleHint

    Shortcut {
        sequences: [StandardKey.Cancel]
        onActivated: rawMessageRoot.close()
    }

    ScrollView {
        anchors.margins: Komai.paddingMedium
        anchors.fill: parent
        padding: Komai.paddingMedium

        TextArea {
            id: rawMessageView

            font: Komai.monospaceFont()
            color: palette.text
            readOnly: true
            textFormat: Text.PlainText

            anchors.fill: parent

            background: Rectangle {
                color: palette.base
            }

        }

    }

    footer: DialogButtonBox {
        standardButtons: DialogButtonBox.Ok
        onAccepted: rawMessageRoot.close()
    }

}
