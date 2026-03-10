// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import QtQuick
import cc.etke.komai

KomaiButton {
    text: qsTr("Open Profile Settings")
    icon.source: "qrc:/icons/icons/ui/person.svg"

    onClicked: MainWindow.showUserSettingsPage(UserSettingsModel.TabAccount)
}
