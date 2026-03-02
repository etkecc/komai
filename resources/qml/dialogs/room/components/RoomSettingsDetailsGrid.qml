// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../moderation"
import "../../../ui"
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import "../../../components" as Components
import cc.etke.komai 1.0

ColumnLayout {
    id: detailsGrid

    required property var roomSettings
    required property var appRoot
    Layout.fillWidth: true
    spacing: 0

    // --- Permissions section ---
    Components.SettingsSection {
        label: qsTr("Permissions")
        Layout.fillWidth: true
        Layout.topMargin: Komai.paddingMedium
        Layout.leftMargin: Komai.paddingMedium
        Layout.rightMargin: Komai.paddingMedium
    }

    // Power levels & permissions
    Item {
        Layout.fillWidth: true
        implicitHeight: plRowContent.implicitHeight
        HoverHandler { id: plRowHover; blocking: false }
        Rectangle { anchors.fill: plRowContent; color: palette.alternateBase; radius: Komai.paddingMedium; visible: plRowHover.hovered; z: -1 }
        RowLayout {
            id: plRowContent
            width: parent.width

            Label {
                text: qsTr("Power levels & permissions")
                color: palette.text
                font.pointSize: 1.1 * Settings.uiFontSizePt
                Layout.fillWidth: true
                Layout.topMargin: Komai.paddingMedium
                Layout.bottomMargin: Komai.paddingMedium
                Layout.leftMargin: Komai.paddingMedium
            }

            Button {
                text: qsTr("Configure")
                onClicked: detailsGrid.appRoot.showPLEditor(detailsGrid.roomSettings)
                Layout.rightMargin: Komai.paddingMedium
            }
        }
    }

    // Aliases
    Item {
        Layout.fillWidth: true
        implicitHeight: aliasRowContent.implicitHeight
        HoverHandler { id: aliasRowHover; blocking: false }
        Rectangle { anchors.fill: aliasRowContent; color: palette.alternateBase; radius: Komai.paddingMedium; visible: aliasRowHover.hovered; z: -1 }
        RowLayout {
            id: aliasRowContent
            width: parent.width

            Label {
                text: qsTr("Aliases")
                color: palette.text
                font.pointSize: 1.1 * Settings.uiFontSizePt
                Layout.fillWidth: true
                Layout.topMargin: Komai.paddingMedium
                Layout.bottomMargin: Komai.paddingMedium
                Layout.leftMargin: Komai.paddingMedium
            }

            Button {
                text: qsTr("Configure")
                onClicked: detailsGrid.appRoot.showAliasEditor(detailsGrid.roomSettings)
                Layout.rightMargin: Komai.paddingMedium
            }
        }
    }

    // Anyone can join
    Item {
        Layout.fillWidth: true
        implicitHeight: publicRowContent.implicitHeight
        HoverHandler { id: publicRowHover; blocking: false }
        Rectangle { anchors.fill: publicRowContent; color: palette.alternateBase; radius: Komai.paddingMedium; visible: publicRowHover.hovered; z: -1 }
        RowLayout {
            id: publicRowContent
            width: parent.width

            Label {
                text: qsTr("Anyone can join")
                color: palette.text
                font.pointSize: 1.1 * Settings.uiFontSizePt
                Layout.fillWidth: true
                Layout.topMargin: Komai.paddingMedium
                Layout.bottomMargin: Komai.paddingMedium
                Layout.leftMargin: Komai.paddingMedium
            }

            ToggleButton {
                id: publicRoomButton
                enabled: detailsGrid.roomSettings.canChangeJoinRules
                checked: !detailsGrid.roomSettings.privateAccess
                Layout.rightMargin: Komai.paddingMedium
            }
        }
    }

    // Allow knocking
    Item {
        Layout.fillWidth: true
        implicitHeight: knockRowContent.implicitHeight
        visible: !publicRoomButton.checked
        HoverHandler { id: knockRowHover; blocking: false }
        Rectangle { anchors.fill: knockRowContent; color: palette.alternateBase; radius: Komai.paddingMedium; visible: knockRowHover.hovered; z: -1 }
        RowLayout {
            id: knockRowContent
            width: parent.width

            Label {
                text: qsTr("Allow knocking")
                color: palette.text
                font.pointSize: 1.1 * Settings.uiFontSizePt
                Layout.fillWidth: true
                Layout.topMargin: Komai.paddingMedium
                Layout.bottomMargin: Komai.paddingMedium
                Layout.leftMargin: Komai.paddingMedium
            }

            ToggleButton {
                id: knockingButton
                enabled: detailsGrid.roomSettings.canChangeJoinRules && detailsGrid.roomSettings.supportsKnocking
                checked: detailsGrid.roomSettings.knockingEnabled
                onCheckedChanged: {
                    if (checked && !detailsGrid.roomSettings.supportsKnockRestricted) restrictedButton.checked = false;
                }
                Layout.rightMargin: Komai.paddingMedium
            }
        }
    }

    // Allow joining via other rooms
    Item {
        Layout.fillWidth: true
        implicitHeight: restrictedRowContent.implicitHeight
        visible: !publicRoomButton.checked
        HoverHandler { id: restrictedRowHover; blocking: false }
        Rectangle { anchors.fill: restrictedRowContent; color: palette.alternateBase; radius: Komai.paddingMedium; visible: restrictedRowHover.hovered; z: -1 }
        RowLayout {
            id: restrictedRowContent
            width: parent.width

            Label {
                text: qsTr("Allow joining via other rooms")
                color: palette.text
                font.pointSize: 1.1 * Settings.uiFontSizePt
                Layout.fillWidth: true
                Layout.topMargin: Komai.paddingMedium
                Layout.bottomMargin: Komai.paddingMedium
                Layout.leftMargin: Komai.paddingMedium
            }

            ToggleButton {
                id: restrictedButton
                enabled: detailsGrid.roomSettings.canChangeJoinRules && detailsGrid.roomSettings.supportsRestricted
                checked: detailsGrid.roomSettings.restrictedEnabled
                onCheckedChanged: {
                    if (checked && !detailsGrid.roomSettings.supportsKnockRestricted) knockingButton.checked = false;
                }
                Layout.rightMargin: Komai.paddingMedium
            }
        }
    }

    // Rooms to join via
    Item {
        Layout.fillWidth: true
        implicitHeight: joinViaRowContent.implicitHeight
        visible: restrictedButton.checked && !publicRoomButton.checked
        HoverHandler { id: joinViaRowHover; blocking: false }
        Rectangle { anchors.fill: joinViaRowContent; color: palette.alternateBase; radius: Komai.paddingMedium; visible: joinViaRowHover.hovered; z: -1 }
        RowLayout {
            id: joinViaRowContent
            width: parent.width

            Label {
                text: qsTr("Rooms to join via")
                color: palette.text
                font.pointSize: 1.1 * Settings.uiFontSizePt
                Layout.fillWidth: true
                Layout.topMargin: Komai.paddingMedium
                Layout.bottomMargin: Komai.paddingMedium
                Layout.leftMargin: Komai.paddingMedium
            }

            Button {
                id: allowedRoomsButton
                enabled: detailsGrid.roomSettings.canChangeJoinRules && detailsGrid.roomSettings.supportsRestricted
                text: qsTr("Change")
                onClicked: detailsGrid.appRoot.showAllowedRoomsEditor(detailsGrid.roomSettings)
                Layout.rightMargin: Komai.paddingMedium
            }
        }
    }

    // Allow guests to join
    Item {
        Layout.fillWidth: true
        implicitHeight: guestRowContent.implicitHeight
        HoverHandler { id: guestRowHover; blocking: false }
        Rectangle { anchors.fill: guestRowContent; color: palette.alternateBase; radius: Komai.paddingMedium; visible: guestRowHover.hovered; z: -1 }
        RowLayout {
            id: guestRowContent
            width: parent.width

            Label {
                text: qsTr("Allow guests to join")
                color: palette.text
                font.pointSize: 1.1 * Settings.uiFontSizePt
                Layout.fillWidth: true
                Layout.topMargin: Komai.paddingMedium
                Layout.bottomMargin: Komai.paddingMedium
                Layout.leftMargin: Komai.paddingMedium
            }

            ToggleButton {
                id: guestAccessButton
                enabled: detailsGrid.roomSettings.canChangeJoinRules
                checked: detailsGrid.roomSettings.guestAccess
                Layout.rightMargin: Komai.paddingMedium
            }
        }
    }

    // Apply access rules button
    Button {
        visible: publicRoomButton.checked == detailsGrid.roomSettings.privateAccess || knockingButton.checked != detailsGrid.roomSettings.knockingEnabled || restrictedButton.checked != detailsGrid.roomSettings.restrictedEnabled || guestAccessButton.checked != detailsGrid.roomSettings.guestAccess || detailsGrid.roomSettings.allowedRoomsModified
        enabled: detailsGrid.roomSettings.canChangeJoinRules
        text: qsTr("Apply access rules")
        onClicked: detailsGrid.roomSettings.changeAccessRules(!publicRoomButton.checked, guestAccessButton.checked, knockingButton.checked, restrictedButton.checked)
        Layout.fillWidth: true
        Layout.leftMargin: Komai.paddingMedium
        Layout.rightMargin: Komai.paddingMedium
    }

    // --- Message visibility section ---
    Components.SettingsSection {
        label: qsTr("Message visibility")
        Layout.fillWidth: true
        Layout.topMargin: Komai.paddingMedium
        Layout.leftMargin: Komai.paddingMedium
        Layout.rightMargin: Komai.paddingMedium
    }

    // Allow viewing history without joining
    Item {
        Layout.fillWidth: true
        implicitHeight: publicHistoryRowContent.implicitHeight
        HoverHandler { id: publicHistoryRowHover; blocking: false }
        Rectangle { anchors.fill: publicHistoryRowContent; color: palette.alternateBase; radius: Komai.paddingMedium; visible: publicHistoryRowHover.hovered; z: -1 }
        ColumnLayout {
            id: publicHistoryRowContent
            width: parent.width
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: Komai.paddingMedium
                Layout.leftMargin: Komai.paddingMedium
                Layout.rightMargin: Komai.paddingMedium

                Label {
                    text: qsTr("Allow viewing history without joining")
                    color: palette.text
                    font.pointSize: 1.1 * Settings.uiFontSizePt
                    Layout.fillWidth: true
                }

                ToggleButton {
                    id: publicHistoryButton
                    enabled: detailsGrid.roomSettings.canChangeHistoryVisibility
                    checked: detailsGrid.roomSettings.historyVisibility == RoomSettings.WorldReadable
                }
            }

            Label {
                text: qsTr("Useful for room previews and public websites.")
                color: palette.buttonText
                font.pointSize: 0.9 * Settings.uiFontSizePt
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingMedium
                Layout.rightMargin: Komai.paddingMedium
                Layout.bottomMargin: Komai.paddingMedium
                wrapMode: Text.Wrap
            }
        }
    }

    // Members can see messages since
    Item {
        Layout.fillWidth: true
        implicitHeight: historyComboRowContent.implicitHeight
        visible: !publicHistoryButton.checked
        HoverHandler { id: historyComboRowHover; blocking: false }
        Rectangle { anchors.fill: historyComboRowContent; color: palette.alternateBase; radius: Komai.paddingMedium; visible: historyComboRowHover.hovered; z: -1 }
        ColumnLayout {
            id: historyComboRowContent
            width: parent.width
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: Komai.paddingMedium
                Layout.leftMargin: Komai.paddingMedium
                Layout.rightMargin: Komai.paddingMedium

                Label {
                    text: qsTr("Members can see messages since")
                    color: palette.text
                    font.pointSize: 1.1 * Settings.uiFontSizePt
                    Layout.fillWidth: true
                }

                ComboBox {
                    id: historyCombo
                    enabled: detailsGrid.roomSettings.canChangeHistoryVisibility

                    model: [
                        qsTr("The beginning"),
                        qsTr("They were invited"),
                        qsTr("They joined")
                    ]

                    property var visibilityValues: [
                        RoomSettings.Shared,
                        RoomSettings.Invited,
                        RoomSettings.Joined
                    ]

                    currentIndex: {
                        var vis = detailsGrid.roomSettings.historyVisibility;
                        if (vis === RoomSettings.Shared) return 0;
                        if (vis === RoomSettings.Invited) return 1;
                        return 2;
                    }
                }
            }

            Label {
                text: qsTr("Changing this won't affect already sent messages, only new ones.")
                color: palette.buttonText
                font.pointSize: 0.9 * Settings.uiFontSizePt
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingMedium
                Layout.rightMargin: Komai.paddingMedium
                Layout.bottomMargin: Komai.paddingMedium
                wrapMode: Text.Wrap
            }
        }
    }

    // Apply visibility changes buttons
    Button {
        visible: !publicHistoryButton.checked && detailsGrid.roomSettings.historyVisibility !== historyCombo.visibilityValues[historyCombo.currentIndex]
        enabled: detailsGrid.roomSettings.canChangeHistoryVisibility
        text: qsTr("Apply visibility changes")
        property int selectedVisibility: publicHistoryButton.checked ? RoomSettings.WorldReadable : historyCombo.visibilityValues[historyCombo.currentIndex]
        onClicked: detailsGrid.roomSettings.changeHistoryVisibility(selectedVisibility)
        Layout.fillWidth: true
        Layout.leftMargin: Komai.paddingMedium
        Layout.rightMargin: Komai.paddingMedium
    }

    Button {
        visible: publicHistoryButton.checked && detailsGrid.roomSettings.historyVisibility !== RoomSettings.WorldReadable
        enabled: detailsGrid.roomSettings.canChangeHistoryVisibility
        text: qsTr("Apply visibility changes")
        onClicked: detailsGrid.roomSettings.changeHistoryVisibility(RoomSettings.WorldReadable)
        Layout.fillWidth: true
        Layout.leftMargin: Komai.paddingMedium
        Layout.rightMargin: Komai.paddingMedium
    }

    // Locally hidden events
    Item {
        Layout.fillWidth: true
        implicitHeight: hiddenEventsRowContent.implicitHeight
        HoverHandler { id: hiddenEventsRowHover; blocking: false }
        Rectangle { anchors.fill: hiddenEventsRowContent; color: palette.alternateBase; radius: Komai.paddingMedium; visible: hiddenEventsRowHover.hovered; z: -1 }
        RowLayout {
            id: hiddenEventsRowContent
            width: parent.width

            Label {
                text: qsTr("Locally hidden events")
                color: palette.text
                font.pointSize: 1.1 * Settings.uiFontSizePt
                Layout.fillWidth: true
                Layout.topMargin: Komai.paddingMedium
                Layout.bottomMargin: Komai.paddingMedium
                Layout.leftMargin: Komai.paddingMedium
            }

            HiddenEventsDialog {
                id: hiddenEventsDialog
                roomid: detailsGrid.roomSettings.roomId
                roomName: detailsGrid.roomSettings.roomName
            }

            Button {
                text: qsTr("Configure")
                onClicked: hiddenEventsDialog.open()
                Layout.rightMargin: Komai.paddingMedium
            }
        }
    }

    // Automatic event deletion
    Item {
        Layout.fillWidth: true
        implicitHeight: eventExpRowContent.implicitHeight
        HoverHandler { id: eventExpRowHover; blocking: false }
        Rectangle { anchors.fill: eventExpRowContent; color: palette.alternateBase; radius: Komai.paddingMedium; visible: eventExpRowHover.hovered; z: -1 }
        RowLayout {
            id: eventExpRowContent
            width: parent.width

            Label {
                text: qsTr("Automatic event deletion")
                color: palette.text
                font.pointSize: 1.1 * Settings.uiFontSizePt
                Layout.fillWidth: true
                Layout.topMargin: Komai.paddingMedium
                Layout.bottomMargin: Komai.paddingMedium
                Layout.leftMargin: Komai.paddingMedium
            }

            EventExpirationDialog {
                id: eventExpirationDialog
                roomid: detailsGrid.roomSettings.roomId
                roomName: detailsGrid.roomSettings.roomName
            }

            Button {
                text: qsTr("Configure")
                onClicked: eventExpirationDialog.open()
                Layout.rightMargin: Komai.paddingMedium
            }
        }
    }

    // --- Extra section ---
    Components.SettingsSection {
        label: qsTr("Extra")
        Layout.fillWidth: true
        Layout.topMargin: Komai.paddingMedium
        Layout.leftMargin: Komai.paddingMedium
        Layout.rightMargin: Komai.paddingMedium
    }

    // Sticker & Emote Settings
    Item {
        Layout.fillWidth: true
        implicitHeight: stickerRowContent.implicitHeight
        HoverHandler { id: stickerRowHover; blocking: false }
        Rectangle { anchors.fill: stickerRowContent; color: palette.alternateBase; radius: Komai.paddingMedium; visible: stickerRowHover.hovered; z: -1 }
        RowLayout {
            id: stickerRowContent
            width: parent.width

            Label {
                text: qsTr("Sticker & Emote Settings")
                color: palette.text
                font.pointSize: 1.1 * Settings.uiFontSizePt
                Layout.fillWidth: true
                Layout.topMargin: Komai.paddingMedium
                Layout.bottomMargin: Komai.paddingMedium
                Layout.leftMargin: Komai.paddingMedium
            }

            Button {
                text: qsTr("Change")
                onClicked: TimelineManager.openImagePackSettings(detailsGrid.roomSettings.roomId)
                Layout.rightMargin: Komai.paddingMedium
            }
        }
    }
}
