// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import cc.etke.komai

RowLayout {
    KomaiButton {
        text: qsTr("Import")
        onClicked: UserSettingsModel.importSessionKeys()
    }
    KomaiButton {
        text: qsTr("Export")
        onClicked: UserSettingsModel.exportSessionKeys()
    }
}
