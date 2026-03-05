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
        text: qsTr("Download")
        onClicked: UserSettingsModel.downloadCrossSigningSecrets()
    }
    KomaiButton {
        text: qsTr("Request")
        onClicked: UserSettingsModel.requestCrossSigningSecrets()
    }
}
