// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components" as Components
import "../../ui"
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import cc.etke.komai 1.0

Components.OverlayDialog {
    id: createRoomRoot

    property bool space: false

    title: space ? qsTr("New community") : qsTr("New Room")
    titleIcon: ":/icons/icons/ui/plus-circle.svg"
    initialFocusItem: newRoomName

    MatrixTextField {
        id: newRoomName

        Layout.fillWidth: true
        label: qsTr("Name")
        placeholderText: qsTr("No name")
    }

    MatrixTextField {
        id: newRoomTopic

        Layout.fillWidth: true
        label: qsTr("Topic")
        placeholderText: qsTr("No topic")
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Komai.paddingMedium

        Label {
            text: "#"
            color: palette.text
        }

        MatrixTextField {
            id: newRoomAlias

            Layout.fillWidth: true
            placeholderText: qsTr("Alias")
        }

        Label {
            property string userName: Komai.currentUser ? Komai.currentUser.userid : ""

            text: userName.substring(userName.indexOf(":"))
            color: palette.text
        }
    }

    GridLayout {
        Layout.fillWidth: true
        columns: 2

        Label {
            Layout.fillWidth: true
            text: qsTr("Public")
            color: palette.text
            HoverHandler {
                id: privateHover
            }
            ToolTip.visible: privateHover.hovered
            ToolTip.text: qsTr("Public rooms can be joined by anyone; private rooms need explicit invites.")
            ToolTip.delay: Komai.tooltipDelay
        }

        ToggleButton {
            id: isPublic

            Layout.alignment: Qt.AlignRight
            checked: false
        }

        Label {
            visible: !createRoomRoot.space
            Layout.fillWidth: true
            text: qsTr("Trusted")
            color: palette.text
            HoverHandler {
                id: trustedHover
            }
            ToolTip.visible: trustedHover.hovered
            ToolTip.text: qsTr("All invitees are given the same power level as the creator")
            ToolTip.delay: Komai.tooltipDelay
        }

        ToggleButton {
            id: isTrusted

            visible: !createRoomRoot.space
            Layout.alignment: Qt.AlignRight
            checked: false
            enabled: !isPublic.checked
        }

        Label {
            visible: !createRoomRoot.space
            Layout.fillWidth: true
            text: qsTr("Encryption")
            color: palette.text
            HoverHandler {
                id: encryptionHover
            }
            ToolTip.visible: encryptionHover.hovered
            ToolTip.text: qsTr("Caution: Encryption cannot be disabled")
            ToolTip.delay: Komai.tooltipDelay
        }

        ToggleButton {
            id: isEncrypted

            visible: !createRoomRoot.space
            Layout.alignment: Qt.AlignRight
            checked: false
        }
    }

    Button {
        Layout.alignment: Qt.AlignRight
        text: qsTr("Create Room")
        highlighted: true
        onClicked: {
            var preset = 0;
            if (isPublic.checked)
                preset = 1;
            else
                preset = isTrusted.checked ? 2 : 0;
            Komai.createRoom(createRoomRoot.space, newRoomName.text, newRoomTopic.text, newRoomAlias.text, isEncrypted.checked, preset);
            createRoomRoot.close();
        }
    }
}
