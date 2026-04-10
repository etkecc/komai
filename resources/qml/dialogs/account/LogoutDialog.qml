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
    id: logoutRoot

    title: qsTr("Sign out")
    titleIcon: ":/icons/icons/ui/power-off.svg"

    Label {
        Layout.fillWidth: true
        color: palette.text
        wrapMode: Text.WordWrap
        text: CallManager.isOnCall ? qsTr("A call is in progress. Sign out?") : qsTr("Are you sure you want to sign out?")
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Komai.paddingMedium

        Components.KomaiButton {
            text: qsTr("Cancel")
            onClicked: logoutRoot.close()
        }

        Item {
            Layout.fillWidth: true
        }

        Components.KomaiButton {
            text: qsTr("Sign out")
            highlighted: true
            onClicked: {
                Komai.logout();
                logoutRoot.close();
            }
        }
    }
}
