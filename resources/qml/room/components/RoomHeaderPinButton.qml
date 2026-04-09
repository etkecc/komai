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

    readonly property int pinCount: room ? room.pinnedMessages.length : 0

    alwaysShowToolTip: true
    toolTipText: qsTr("Show pinned messages")
    image: ":/icons/icons/ui/pin.svg"
    labelText: qsTr("Pins (%1)").arg(pinCount)
    showLabel: showTextLabel
    visible: !!room && pinCount > 0

    onClicked: {
        const component = Qt.createComponent("qrc:/resources/qml/dialogs/timeline/PinnedMessagesDialog.qml");
        if (component.status !== Component.Ready) {
            console.error("PinnedMessagesDialog: " + component.errorString());
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
