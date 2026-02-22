// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import "../../dialogs"

Button {
    id: logoutBtn
    text: qsTr("Logout")
    icon.source: "qrc:/icons/icons/ui/power-off.svg"

    onClicked: {
        var dialog = logoutDialog.createObject();
        dialog.open();
        destroyOnClose(dialog);
    }

    Component {
        id: logoutDialog
        LogoutDialog {}
    }
}
