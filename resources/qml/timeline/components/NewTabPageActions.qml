// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai
import "../../shell/components" as ShellComponents

GridLayout {
    id: root

    required property var dialogHost

    // Measure natural content widths without circular bindings.
    readonly property real uniformWidth: {
        const pad = exploreButton.leftPadding + exploreButton.rightPadding;
        return Math.max(
            exploreButton.contentItem.implicitWidth + pad,
            joinButton.contentItem.implicitWidth + pad,
            newButton.contentItem.implicitWidth + pad);
    }

    // Natural width when all three buttons sit side-by-side on a single row.
    readonly property real preferredRowWidth: uniformWidth * 3 + columnSpacing * 2

    // Drop to one-button-per-row when the row no longer fits at its natural size.
    readonly property bool stackVertically: width > 0 && width < preferredRowWidth

    columns: stackVertically ? 1 : 3
    rowSpacing: Komai.paddingMedium
    columnSpacing: Komai.paddingMedium

    KomaiButton {
        id: joinButton

        Layout.fillWidth: true
        Layout.preferredWidth: root.uniformWidth
        text: qsTr("Join room")
        toolTipText: qsTr("Join an existing room by address or alias")
        icon.source: "qrc:/icons/icons/ui/arrow-join.svg"
        font.pointSize: Settings.uiFontSizePt * 1.2
        onClicked: Komai.openJoinRoomDialog()
    }

    KomaiButton {
        id: exploreButton

        Layout.fillWidth: true
        Layout.preferredWidth: root.uniformWidth
        text: qsTr("Explore public rooms")
        toolTipText: qsTr("Browse the public room directory")
        icon.source: "qrc:/icons/icons/ui/compass-northwest.svg"
        font.pointSize: Settings.uiFontSizePt * 1.2
        onClicked: root.dialogHost.openRoomDirectory()
    }

    KomaiButton {
        id: newButton

        Layout.fillWidth: true
        Layout.preferredWidth: root.uniformWidth
        text: qsTr("New room/space")
        toolTipText: qsTr("Create a new room or space [Ctrl+N]")
        icon.source: "qrc:/icons/icons/ui/plus-circle.svg"
        font.pointSize: Settings.uiFontSizePt * 1.2
        onClicked: newDialog.open()
    }

    ShellComponents.RoomJoinCreateDialog {
        id: newDialog
        dialogHost: root.dialogHost
    }
}
