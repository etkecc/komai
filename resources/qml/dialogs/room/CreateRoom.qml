// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components" as Components
import "../../ui"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai 1.0

Components.OverlayDialog {
    id: createRoomRoot

    property bool space: false

    title: space ? qsTr("New space") : qsTr("New room")
    titleIcon: space ? ":/icons/icons/ui/squares-nested.svg" : ":/icons/icons/ui/people-community.svg"
    initialFocusItem: newRoomName
    overlayDialogMinWidth: 620

    Components.KomaiTextField {
        id: newRoomName

        Layout.fillWidth: true
        placeholderText: qsTr("Name")
    }

    Components.KomaiTextField {
        id: newRoomTopic

        Layout.fillWidth: true
        placeholderText: qsTr("Topic")
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Komai.paddingMedium

        Label {
            text: "#"
            color: palette.text
        }

        Components.KomaiTextField {
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

    // Public
    Item {
        Layout.fillWidth: true
        implicitHeight: publicRowContent.implicitHeight
        HoverHandler { id: publicRowHover; blocking: false }
        Rectangle { anchors.fill: publicRowContent; color: palette.window; radius: Komai.paddingMedium; visible: publicRowHover.hovered; z: -1 }
        ColumnLayout {
            id: publicRowContent
            width: parent.width
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: Komai.paddingMedium
                Layout.leftMargin: Komai.paddingMedium
                Layout.rightMargin: Komai.paddingMedium
                Layout.bottomMargin: Komai.paddingSmall

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Public")
                    color: palette.text
                }

                ToggleButton {
                    id: isPublic

                    Layout.alignment: Qt.AlignRight
                    checked: false
                }
            }

            Label {
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingMedium
                Layout.rightMargin: Komai.paddingMedium
                Layout.bottomMargin: Komai.paddingMedium
                text: createRoomRoot.space ? qsTr("Anyone can join a public space. Private spaces require an invite.") : qsTr("Anyone can join a public room. Private rooms require an invite.")
                color: palette.buttonText
                font.pointSize: Settings.uiFontSizePt
                wrapMode: Text.Wrap
            }
        }
    }

    // Trusted
    Item {
        Layout.fillWidth: true
        implicitHeight: trustedRowContent.implicitHeight
        visible: !createRoomRoot.space
        HoverHandler { id: trustedRowHover; blocking: false }
        Rectangle { anchors.fill: trustedRowContent; color: palette.window; radius: Komai.paddingMedium; visible: trustedRowHover.hovered; z: -1 }
        ColumnLayout {
            id: trustedRowContent
            width: parent.width
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: Komai.paddingMedium
                Layout.leftMargin: Komai.paddingMedium
                Layout.rightMargin: Komai.paddingMedium
                Layout.bottomMargin: Komai.paddingSmall

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Trusted")
                    color: palette.text
                }

                ToggleButton {
                    id: isTrusted

                    Layout.alignment: Qt.AlignRight
                    checked: false
                    enabled: !isPublic.checked
                }
            }

            Label {
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingMedium
                Layout.rightMargin: Komai.paddingMedium
                Layout.bottomMargin: Komai.paddingMedium
                text: qsTr("Invitees get the same power level as the room creator.")
                color: palette.buttonText
                font.pointSize: Settings.uiFontSizePt
                wrapMode: Text.Wrap
            }
        }
    }

    // Encryption
    Item {
        Layout.fillWidth: true
        implicitHeight: encryptionRowContent.implicitHeight
        visible: !createRoomRoot.space
        HoverHandler { id: encryptionRowHover; blocking: false }
        Rectangle { anchors.fill: encryptionRowContent; color: palette.window; radius: Komai.paddingMedium; visible: encryptionRowHover.hovered; z: -1 }
        ColumnLayout {
            id: encryptionRowContent
            width: parent.width
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: Komai.paddingMedium
                Layout.leftMargin: Komai.paddingMedium
                Layout.rightMargin: Komai.paddingMedium
                Layout.bottomMargin: Komai.paddingSmall

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Encryption")
                    color: palette.text
                }

                ToggleButton {
                    id: isEncrypted

                    Layout.alignment: Qt.AlignRight
                    checked: false
                }
            }

            Label {
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingMedium
                Layout.rightMargin: Komai.paddingMedium
                Layout.bottomMargin: Komai.paddingMedium
                text: qsTr("Only participants can read messages. Cannot be disabled once enabled.")
                color: palette.buttonText
                font.pointSize: Settings.uiFontSizePt
                wrapMode: Text.Wrap
            }
        }
    }

    // Warning: public + encrypted
    Label {
        Layout.fillWidth: true
        Layout.preferredWidth: 0
        Layout.leftMargin: Komai.paddingMedium
        Layout.rightMargin: Komai.paddingMedium
        visible: isPublic.checked && isEncrypted.checked && !createRoomRoot.space
        text: qsTr("Encryption has a high cost in public rooms with many participants.")
        color: Komai.theme.attention
        font.pointSize: Settings.uiFontSizePt
        wrapMode: Text.Wrap
    }

    Components.KomaiButton {
        Layout.alignment: Qt.AlignRight
        text: qsTr("Create")
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
