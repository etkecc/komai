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
    id: root

    required property string eventId

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

        TextField {
            id: reason

            Layout.fillWidth: true
        }

        Label {
            text: qsTr("How bad is the message?")
            color: palette.text
        }

        Slider {
            id: score

            from: 0
            to: -100
            stepSize: 25
            snapMode: Slider.SnapAlways
            Layout.fillWidth: true
        }

        Item {}

        Label {
            color: palette.text
            text: {
                if (score.value === 0)
                    return qsTr("Not bad")
                else if (score.value === -25)
                    return qsTr("Mild")
                else if (score.value === -50)
                    return qsTr("Bad")
                else if (score.value === -75)
                    return qsTr("Serious")
                else if (score.value === -100)
                    return qsTr("Extremely serious")
            }
        }
    }

    Button {
        Layout.alignment: Qt.AlignRight
        text: qsTr("Report")
        highlighted: true
        onClicked: {
            room.reportEvent(root.eventId, reason.text, score.value);
            root.close();
        }
    }
}
