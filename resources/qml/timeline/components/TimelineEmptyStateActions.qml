// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai

RowLayout {
    id: root

    required property var dialogHost

    spacing: Komai.paddingMedium

    KomaiButton {
        id: joinButton

        Layout.preferredWidth: root.uniformWidth
        text: qsTr("Join room")
        icon.source: "qrc:/icons/icons/ui/arrow-join.svg"
        onClicked: Komai.openJoinRoomDialog()
    }

    KomaiButton {
        id: exploreButton

        Layout.preferredWidth: root.uniformWidth
        text: qsTr("Explore public rooms")
        icon.source: "qrc:/icons/icons/ui/compass-northwest.svg"
        onClicked: root.dialogHost.openRoomDirectory()
    }

    KomaiButton {
        id: newButton

        Layout.preferredWidth: root.uniformWidth
        text: qsTr("New")
        icon.source: "qrc:/icons/icons/ui/plus-circle.svg"
        onClicked: newMenu.popup(newButton)
    }

    Menu {
        id: newMenu

        MenuItem {
            text: qsTr("New direct chat")
            icon.source: "qrc:/icons/icons/ui/person.svg"
            onTriggered: root.dialogHost.openCatalogDialog(
                "qrc:/resources/qml/dialogs/room/CreateDirect.qml", {})
        }

        MenuItem {
            text: qsTr("New room")
            icon.source: "qrc:/icons/icons/ui/people-community.svg"
            onTriggered: root.dialogHost.openCatalogDialog(
                "qrc:/resources/qml/dialogs/room/CreateRoom.qml", {})
        }

        MenuItem {
            text: qsTr("New space")
            icon.source: "qrc:/icons/icons/ui/squares-nested.svg"
            onTriggered: root.dialogHost.openCatalogDialog(
                "qrc:/resources/qml/dialogs/room/CreateRoom.qml", {"space": true})
        }
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
