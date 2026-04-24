// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../components" as Components
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai 1.0

Components.OverlayDialog {
    id: closeToTrayRoot

    title: qsTr("Quit completely or close to tray?")
    titleIcon: ":/icons/icons/ui/power-off.svg"

    Label {
        Layout.fillWidth: true
        color: palette.text
        wrapMode: Text.WordWrap
        text: qsTr("Komai can keep running in the system tray so you keep getting notifications and can open it quickly.")
    }

    Label {
        Layout.fillWidth: true
        color: palette.text
        wrapMode: Text.WordWrap
        text: qsTr("We won't ask again. You can change this later in Settings.")
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Komai.paddingMedium

        Components.KomaiButton {
            text: qsTr("Cancel")
            onClicked: closeToTrayRoot.close()
        }

        Item {
            Layout.fillWidth: true
        }

        Components.KomaiButton {
            text: qsTr("Close to tray")
            onClicked: {
                Komai.acceptCloseToTrayAsTray();
                closeToTrayRoot.close();
            }
        }

        Components.KomaiButton {
            text: qsTr("Quit")
            highlighted: true
            onClicked: {
                Komai.acceptCloseToTrayAsQuit();
                closeToTrayRoot.close();
            }
        }
    }
}
