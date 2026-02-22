// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import im.nheko
import "../../dialogs"

RowLayout {
    Button {
        text: qsTr("DOWNLOAD")
        onClicked: UserSettingsModel.downloadCrossSigningSecrets()
    }
    Button {
        text: qsTr("REQUEST")
        onClicked: UserSettingsModel.requestCrossSigningSecrets()
    }
}
