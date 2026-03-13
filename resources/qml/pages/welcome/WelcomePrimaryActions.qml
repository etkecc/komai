// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15
import QtQuick.Layouts 1.2
import cc.etke.komai 1.0
import "../../components/"

RowLayout {
    id: actions

    signal registerRequested
    signal loginRequested

    Item {
        Layout.fillWidth: true
    }

    KomaiButton {
        Layout.margins: Komai.paddingLarge
        Layout.alignment: Qt.AlignHCenter
        text: qsTr("Register")
        icon.source: "qrc:/icons/icons/ui/plus-circle.svg"
        highlighted: true
        onClicked: actions.registerRequested()
    }

    KomaiButton {
        Layout.margins: Komai.paddingLarge
        Layout.alignment: Qt.AlignHCenter
        text: qsTr("Login")
        icon.source: "qrc:/icons/icons/ui/arrow-right.svg"
        highlighted: true
        onClicked: actions.loginRequested()
    }

    Item {
        Layout.fillWidth: true
    }
}
