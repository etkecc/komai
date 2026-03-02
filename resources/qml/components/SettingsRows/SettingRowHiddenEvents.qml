// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import cc.etke.komai
import "../../dialogs/moderation"

Button {
    text: qsTr("Configure")
    onClicked: {
        var dialog = hiddenEventsDialog.createObject();
        dialog.open();
        destroyOnClose(dialog);
    }

    Component {
        id: hiddenEventsDialog

        HiddenEventsDialog {}
    }
}
