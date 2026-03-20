// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../components" as Components
import QtQuick 2.3
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.10

ColumnLayout {
    property string title: qsTr("Verification Complete")

    spacing: 16

    Label {
        Layout.fillWidth: true
        wrapMode: Text.Wrap
        text: qsTr("Verification complete. Both devices have been verified.")
        color: palette.text
    }

    RowLayout {
        Item {
            Layout.fillWidth: true
        }

        Components.KomaiButton {
            Layout.alignment: Qt.AlignRight
            text: qsTr("Close")
            onClicked: dialog.close()
        }

    }

}
