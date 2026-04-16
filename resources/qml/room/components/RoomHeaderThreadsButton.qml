// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import cc.etke.komai

RoomHeaderActionButton {
    id: root

    required property var room
    required property string roomId
    property bool showTextLabel: false

    alwaysShowToolTip: true
    toolTipText: qsTr("Show threads")
    image: ":/icons/icons/ui/thread.svg"
    labelText: qsTr("Threads")
    showLabel: showTextLabel
    visible: !!room

    onClicked: {
        const component = Qt.createComponent("qrc:/resources/qml/dialogs/timeline/ThreadsDialog.qml");
        if (component.status !== Component.Ready) {
            console.error("ThreadsDialog: " + component.errorString());
            return;
        }

        const dialog = component.createObject(root, {
            "room": root.room,
            "roomId": root.roomId
        });
        if (!dialog)
            return;

        dialog.open();
        if (dialog.closing !== undefined)
            dialog.closing.connect(() => dialog.destroy(1000));
        else if (dialog.aboutToHide !== undefined)
            dialog.aboutToHide.connect(() => dialog.destroy(1000));
    }
}
