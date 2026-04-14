// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai
import "../../shell/components" as ShellComponents

RowLayout {
    id: root

    required property var dialogHost

    spacing: Komai.paddingMedium

    KomaiButton {
        id: joinButton

        Layout.preferredWidth: root.uniformWidth
        text: qsTr("Join room")
        toolTipText: qsTr("Join an existing room by address or alias")
        icon.source: "qrc:/icons/icons/ui/arrow-join.svg"
        onClicked: Komai.openJoinRoomDialog()
    }

    KomaiButton {
        id: exploreButton

        Layout.preferredWidth: root.uniformWidth
        text: qsTr("Explore public rooms")
        toolTipText: qsTr("Browse the public room directory")
        icon.source: "qrc:/icons/icons/ui/compass-northwest.svg"
        onClicked: root.dialogHost.openRoomDirectory()
    }

    KomaiButton {
        id: newButton

        Layout.preferredWidth: root.uniformWidth
        text: qsTr("New room/space")
        toolTipText: qsTr("Create a new room or space [Ctrl+N]")
        icon.source: "qrc:/icons/icons/ui/plus-circle.svg"
        onClicked: newDialog.open()
    }

    ShellComponents.RoomJoinCreateDialog {
        id: newDialog
        dialogHost: root.dialogHost
    }

    // Measure natural content widths without circular bindings.
    readonly property real uniformWidth: {
        const pad = exploreButton.leftPadding + exploreButton.rightPadding;
        return Math.max(
            exploreButton.contentItem.implicitWidth + pad,
            joinButton.contentItem.implicitWidth + pad,
            newButton.contentItem.implicitWidth + pad);
    }
}
