// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai

RowLayout {
    Button {
        text: qsTr("Download")
        onClicked: UserSettingsModel.downloadCrossSigningSecrets()
    }
    Button {
        text: qsTr("Request")
        onClicked: UserSettingsModel.requestCrossSigningSecrets()
    }
}
