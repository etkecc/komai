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
    id: root

    required property string eventId
    required property var room

    title: qsTr("Report message")
    titleIcon: ":/icons/icons/ui/alert.svg"
    initialFocusItem: reason

    Label {
        Layout.fillWidth: true
        color: palette.text
        wrapMode: Label.WordWrap
        text: qsTr("This message you are reporting will be sent to your server administrator for review. Please note that not all server administrators review reported content. You should also ask a room moderator to remove the content if necessary.")
    }

    GridLayout {
        Layout.fillWidth: true
        columnSpacing: Komai.paddingMedium
        rowSpacing: Komai.paddingMedium
        columns: 2

        Label {
            text: qsTr("Enter your reason for reporting:")
            color: palette.text
        }

        Components.KomaiTextField {
            id: reason

            Layout.fillWidth: true
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Komai.paddingMedium

        Components.KomaiButton {
            text: qsTr("Cancel")
            onClicked: root.close()
        }

        Item {
            Layout.fillWidth: true
        }

        Components.KomaiButton {
            text: qsTr("Report")
            highlighted: true
            onClicked: {
                room.reportEvent(root.eventId, reason.text);
                root.close();
            }
        }
    }
}
