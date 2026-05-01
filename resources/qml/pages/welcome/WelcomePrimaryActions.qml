// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import cc.etke.komai 1.0
import "../../components/"

RowLayout {
    id: actions

    signal newToMatrixRequested
    signal registerRequested
    signal loginRequested

    Layout.alignment: Qt.AlignHCenter
    Layout.topMargin: Komai.paddingLarge

    Item {
        Layout.fillWidth: true
    }

    KomaiButton {
        Layout.margins: Komai.paddingLarge
        Layout.alignment: Qt.AlignVCenter
        text: qsTr("New to Matrix?")
        icon.source: "qrc:/icons/icons/ui/compass-northwest.svg"
        font.pointSize: Settings.uiFontSizePt * 1.3
        onClicked: actions.newToMatrixRequested()
        Keys.onReturnPressed: actions.newToMatrixRequested()
        Keys.onEnterPressed: actions.newToMatrixRequested()
    }

    KomaiButton {
        Layout.margins: Komai.paddingLarge
        Layout.alignment: Qt.AlignVCenter
        text: qsTr("Register")
        icon.source: "qrc:/icons/icons/ui/plus-circle.svg"
        font.pointSize: Settings.uiFontSizePt * 1.3
        highlighted: true
        onClicked: actions.registerRequested()
        Keys.onReturnPressed: actions.registerRequested()
        Keys.onEnterPressed: actions.registerRequested()
    }

    KomaiButton {
        Layout.margins: Komai.paddingLarge
        Layout.alignment: Qt.AlignVCenter
        text: qsTr("Sign in")
        icon.source: "qrc:/icons/icons/ui/arrow-right.svg"
        font.pointSize: Settings.uiFontSizePt * 1.3
        highlighted: true
        onClicked: actions.loginRequested()
        Keys.onReturnPressed: actions.loginRequested()
        Keys.onEnterPressed: actions.loginRequested()
    }

    Item {
        Layout.fillWidth: true
    }
}
