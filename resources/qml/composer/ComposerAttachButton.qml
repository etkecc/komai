// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import "../ui"

ComposerToolbarButton {
    id: root

    required property var room
    required property bool showAllButtons
    readonly property bool uploadInProgress: !!(root.room && root.room.input && root.room.input.uploading === true)

    Layout.alignment: Qt.AlignBottom
    ToolTip.text: qsTr("Send a file")
    image: ":/icons/icons/ui/attach.svg"
    visible: showAllButtons

    onClicked: {
        if (room && room.input)
            room.input.openFileSelection();
    }

    Rectangle {
        anchors.fill: parent
        color: palette.window
        visible: root.uploadInProgress

        Spinner {
            anchors.centerIn: parent
            height: parent.height / 2
            running: root.uploadInProgress
        }
    }
}
