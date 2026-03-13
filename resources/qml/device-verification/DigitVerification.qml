// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../components" as Components
import QtQuick 2.3
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.10
import cc.etke.komai 1.0

ColumnLayout {
    property string title: qsTr("Verification Code")

    spacing: 16

    Label {
        Layout.preferredWidth: 400
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        text: qsTr("Please verify the following digits. You should see the same numbers on both sides. If they differ, please press 'They do not match!' to abort verification!")
        color: palette.text
        verticalAlignment: Text.AlignVCenter
    }

    Item { Layout.fillHeight: true; }
    RowLayout {
        Layout.alignment: Qt.AlignHCenter

        Label {
            font.pointSize: Settings.uiFontSizePt * 2
            text: flow.sasList[0]
            color: palette.text
        }

        Label {
            font.pointSize: Settings.uiFontSizePt * 2
            text: flow.sasList[1]
            color: palette.text
        }

        Label {
            font.pointSize: Settings.uiFontSizePt * 2
            text: flow.sasList[2]
            color: palette.text
        }

    }
    Item { Layout.fillHeight: true; }

    RowLayout {
        Components.KomaiButton {
            Layout.alignment: Qt.AlignLeft
            text: qsTr("They do not match!")
            onClicked: {
                flow.cancel();
                dialog.close();
            }
        }

        Item {
            Layout.fillWidth: true
        }

        Components.KomaiButton {
            Layout.alignment: Qt.AlignRight
            text: qsTr("They match!")
            onClicked: flow.next()
        }

    }

}
