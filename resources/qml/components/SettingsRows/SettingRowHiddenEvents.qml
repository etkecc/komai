// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import im.nheko
import "../../dialogs"

Button {
    text: qsTr("CONFIGURE")
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
