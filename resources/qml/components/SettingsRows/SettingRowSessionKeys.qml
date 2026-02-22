// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import im.nheko

RowLayout {
    Button {
        text: qsTr("IMPORT")
        onClicked: UserSettingsModel.importSessionKeys()
    }
    Button {
        text: qsTr("EXPORT")
        onClicked: UserSettingsModel.exportSessionKeys()
    }
}
