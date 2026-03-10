// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import QtQuick
import cc.etke.komai
import "../../dialogs/moderation"

KomaiButton {
    text: qsTr("Configure")
    onClicked: {
        var dialog = hiddenEventsDialog.createObject(this);
        dialog.open();
        destroyOnClose(dialog);
    }

    Component {
        id: hiddenEventsDialog

        HiddenEventsDialog {}
    }
}
