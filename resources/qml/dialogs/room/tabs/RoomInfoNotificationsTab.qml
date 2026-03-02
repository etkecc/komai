// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import cc.etke.komai 1.0

Item {
    id: notificationsTab

    property var roomSettings
    property var members
    property var room
    property var appRoot

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: Komai.paddingLarge
        spacing: Komai.paddingMedium

        Label {
            text: qsTr("Configure how you receive notifications for this room.")
            color: palette.text
            wrapMode: Text.WordWrap
            Layout.fillWidth: true
        }

        Label {
            text: qsTr("Notifications")
            color: palette.text
            font.bold: true
            Layout.topMargin: Komai.paddingMedium
        }

        ComboBox {
            id: notificationsCombo

            Layout.fillWidth: true
            model: [qsTr("Muted"), qsTr("Mentions only"), qsTr("All messages")]
            currentIndex: notificationsTab.roomSettings ? notificationsTab.roomSettings.notifications : 0
            onActivated: (index) => {
                if (notificationsTab.roomSettings)
                    notificationsTab.roomSettings.changeNotifications(index);
            }
            wheelEnabled: activeFocus
        }

        Item {
            Layout.fillHeight: true
        }
    }
}
