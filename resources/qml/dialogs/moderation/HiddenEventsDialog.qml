// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components" as Components
import "../../ui"
import QtQuick
import QtQuick.Layouts
import cc.etke.komai 1.0

Components.OverlayDialog {
    id: hiddenEventsDialog

    property string roomid: ""
    overlayDialogMinWidth: 720
    overlayDialogMaxWidthRatio: 0.9

    title: roomid ? qsTr("Hidden events in this room") : qsTr("Hidden events")
    titleIcon: ":/icons/icons/ui/settings.svg"

    MatrixText {
        text: roomid
            ? qsTr("Choose which extra events are <b>shown</b> in this room:")
            : qsTr("Choose which extra events are <b>shown</b> in all rooms:")
        font.pixelSize: Math.floor(fontMetrics.font.pixelSize * 1.2)
        Layout.fillWidth: true
    }

    Components.HiddenEventsSettingsContent {
        id: hiddenEventsContent

        Layout.fillWidth: true
        roomId: hiddenEventsDialog.roomid
        autoSave: false
    }

    Components.KomaiButton {
        Layout.alignment: Qt.AlignRight
        text: qsTr("Save")
        highlighted: true
        onClicked: {
            hiddenEventsContent.save();
            hiddenEventsDialog.close();
        }
    }
}
