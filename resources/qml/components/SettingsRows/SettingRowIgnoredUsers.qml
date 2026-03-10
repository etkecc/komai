// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import QtQuick
import cc.etke.komai
import "../../dialogs/moderation"

KomaiButton {
    text: qsTr("Manage")
    onClicked: {
        var dialog = ignoredUsersDialog.createObject(this);
        dialog.open();
        destroyOnClose(dialog);
    }

    Component {
        id: ignoredUsersDialog
        IgnoredUsers {}
    }
}
