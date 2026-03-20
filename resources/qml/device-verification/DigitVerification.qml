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
    property string title: qsTr("Do both devices show the same sequence of numbers?")

    spacing: 16

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
            highlighted: true
            text: qsTr("They match!")
            onClicked: flow.next()
        }

    }

}
