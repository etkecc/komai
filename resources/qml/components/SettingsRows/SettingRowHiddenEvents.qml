// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import im.nheko
import "../../dialogs"
import "../../dialogs/moderation"

Button {
    text: qsTr("Configure")
    onClicked: {
        var dialog = hiddenEventsDialog.createObject();
        dialog.show();
        destroyOnClose(dialog);
    }

    Component {
        id: hiddenEventsDialog

        HiddenEventsDialog {}
    }
}
