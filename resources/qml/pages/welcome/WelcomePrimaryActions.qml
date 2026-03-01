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

    FlatButton {
        compact: true
        Layout.margins: Komai.paddingLarge
        Layout.alignment: Qt.AlignHCenter
        text: qsTr("REGISTER")
        iconImage: "image://colorimage/:/icons/icons/ui/plus-circle.svg?" + (enabled ? palette.light : palette.buttonText)
        onClicked: actions.registerRequested()
    }

    FlatButton {
        compact: true
        Layout.margins: Komai.paddingLarge
        Layout.alignment: Qt.AlignHCenter
        text: qsTr("LOGIN")
        iconImage: "image://colorimage/:/icons/icons/ui/arrow-right.svg?" + (enabled ? palette.light : palette.buttonText)
        onClicked: actions.loginRequested()
    }

    Item {
        Layout.fillWidth: true
    }
}
