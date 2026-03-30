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
    spacing: Komai.paddingSmall

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
        Layout.leftMargin: Komai.paddingMedium
        Layout.rightMargin: Komai.paddingMedium
        implicitHeight: plRowContent.implicitHeight
        HoverHandler { id: plRowHover; blocking: false }
        Rectangle { anchors.fill: plRowContent; color: plRowHover.hovered ? palette.dark : palette.window; radius: Komai.paddingMedium; z: -1 }
        RowLayout {
            id: plRowContent
            width: parent.width

            Label {
                text: qsTr("Power levels & permissions")
                color: plRowHover.hovered ? palette.brightText : palette.text
                font.pointSize: 1.1 * Settings.uiFontSizePt
                Layout.fillWidth: true
                Layout.topMargin: Komai.paddingMedium
                Layout.bottomMargin: Komai.paddingMedium
                Layout.leftMargin: Komai.paddingMedium
            }

            Components.KomaiButton {
                text: qsTr("Configure")
                onClicked: detailsGrid.appRoot.showPLEditor(detailsGrid.roomSettings)
                Layout.rightMargin: Komai.paddingMedium
            }
        }
    }

    // Aliases
    Item {
        Layout.fillWidth: true
        Layout.leftMargin: Komai.paddingMedium
        Layout.rightMargin: Komai.paddingMedium
        implicitHeight: aliasRowContent.implicitHeight
        HoverHandler { id: aliasRowHover; blocking: false }
        Rectangle { anchors.fill: aliasRowContent; color: aliasRowHover.hovered ? palette.dark : palette.window; radius: Komai.paddingMedium; z: -1 }
        ColumnLayout {
            id: aliasRowContent
            width: parent.width
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: Komai.paddingMedium
                Layout.leftMargin: Komai.paddingMedium
                Layout.rightMargin: Komai.paddingMedium

                Label {
                    text: qsTr("Aliases")
                    color: aliasRowHover.hovered ? palette.brightText : palette.text
                    font.pointSize: 1.1 * Settings.uiFontSizePt
                    Layout.fillWidth: true
                }

                Components.KomaiButton {
                    text: qsTr("Configure")
                    onClicked: detailsGrid.appRoot.showAliasEditor(detailsGrid.roomSettings)
                }
            }

            Label {
                text: qsTr("<a href='https://spec.matrix.org/v1.17/client-server-api/#room-aliases'>Aliases</a> are alternative addresses (like #room:example.com) that people can use to find this room.")
                color: aliasRowHover.hovered ? palette.brightText : palette.buttonText
                font.pointSize: 0.9 * Settings.uiFontSizePt
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingMedium
                Layout.rightMargin: Komai.paddingMedium
                Layout.bottomMargin: Komai.paddingMedium
                wrapMode: Text.Wrap
                textFormat: Text.RichText
                onLinkActivated: function(link) { Qt.openUrlExternally(link); }
            }
        }
    }

    // Room access
    Item {
        Layout.fillWidth: true
        Layout.leftMargin: Komai.paddingMedium
        Layout.rightMargin: Komai.paddingMedium
        implicitHeight: publicRowContent.implicitHeight
        HoverHandler { id: publicRowHover; blocking: false }
        Rectangle { anchors.fill: publicRowContent; color: publicRowHover.hovered ? palette.dark : palette.window; radius: Komai.paddingMedium; z: -1 }
        RowLayout {
            id: publicRowContent
            width: parent.width

            Label {
                text: qsTr("Room access")
                color: publicRowHover.hovered ? palette.brightText : palette.text
                font.pointSize: 1.1 * Settings.uiFontSizePt
                Layout.fillWidth: true
                Layout.topMargin: Komai.paddingMedium
                Layout.bottomMargin: Komai.paddingMedium
                Layout.leftMargin: Komai.paddingMedium
            }

            Components.KomaiComboBox {
                id: accessCombo
                enabled: detailsGrid.roomSettings.canChangeJoinRules

                readonly property bool isPrivate: currentIndex === 1

                model: [
                    qsTr("Public (anyone can join)"),
                    qsTr("Private (invite only)")
                ]

                currentIndex: detailsGrid.roomSettings.privateAccess ? 1 : 0

                Layout.rightMargin: Komai.paddingMedium
            }
        }
    }

    // Allow knocking
    Item {
        Layout.fillWidth: true
        Layout.leftMargin: Komai.paddingMedium
        Layout.rightMargin: Komai.paddingMedium
        implicitHeight: knockRowContent.implicitHeight
        visible: accessCombo.isPrivate
        HoverHandler { id: knockRowHover; blocking: false }
        Rectangle { anchors.fill: knockRowContent; color: knockRowHover.hovered ? palette.dark : palette.window; radius: Komai.paddingMedium; z: -1 }
        ColumnLayout {
            id: knockRowContent
            width: parent.width
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: Komai.paddingMedium
                Layout.leftMargin: Komai.paddingMedium
                Layout.rightMargin: Komai.paddingMedium

                Label {
                    text: qsTr("Allow knocking")
                    color: knockRowHover.hovered ? palette.brightText : palette.text
                    font.pointSize: 1.1 * Settings.uiFontSizePt
                    Layout.fillWidth: true
                }

                ToggleButton {
                    id: knockingButton
                    textColor: knockRowHover.hovered ? palette.brightText : palette.buttonText
                    enabled: detailsGrid.roomSettings.canChangeJoinRules && detailsGrid.roomSettings.supportsKnocking
                    checked: detailsGrid.roomSettings.knockingEnabled
                    onCheckedChanged: {
                        if (checked && !detailsGrid.roomSettings.supportsKnockRestricted) restrictedButton.checked = false;
                    }
                }
            }

            Label {
                text: qsTr("Non-members can <a href='https://spec.matrix.org/v1.17/client-server-api/#knocking-on-rooms'>request to join</a>. Users with invite permission can accept.")
                color: knockRowHover.hovered ? palette.brightText : palette.buttonText
                font.pointSize: 0.9 * Settings.uiFontSizePt
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingMedium
                Layout.rightMargin: Komai.paddingMedium
                Layout.bottomMargin: Komai.paddingMedium
                wrapMode: Text.Wrap
                textFormat: Text.RichText
                onLinkActivated: function(link) { Qt.openUrlExternally(link); }
            }
        }
    }

    // Allow joining from Spaces
    Item {
        Layout.fillWidth: true
        Layout.leftMargin: Komai.paddingMedium
        Layout.rightMargin: Komai.paddingMedium
        implicitHeight: restrictedRowContent.implicitHeight
        visible: accessCombo.isPrivate
        HoverHandler { id: restrictedRowHover; blocking: false }
        Rectangle { anchors.fill: restrictedRowContent; color: restrictedRowHover.hovered ? palette.dark : palette.window; radius: Komai.paddingMedium; z: -1 }
        ColumnLayout {
            id: restrictedRowContent
            width: parent.width
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: Komai.paddingMedium
                Layout.leftMargin: Komai.paddingMedium
                Layout.rightMargin: Komai.paddingMedium

                Label {
                    text: qsTr("Allow joining from Spaces")
                    color: restrictedRowHover.hovered ? palette.brightText : palette.text
                    font.pointSize: 1.1 * Settings.uiFontSizePt
                    Layout.fillWidth: true
                }

                ToggleButton {
                    id: restrictedButton
                    textColor: restrictedRowHover.hovered ? palette.brightText : palette.buttonText
                    enabled: detailsGrid.roomSettings.canChangeJoinRules && detailsGrid.roomSettings.supportsRestricted
                    checked: detailsGrid.roomSettings.restrictedEnabled
                    onCheckedChanged: {
                        if (checked && !detailsGrid.roomSettings.supportsKnockRestricted) knockingButton.checked = false;
                    }
                }
            }

            Label {
                text: qsTr("Members of selected Spaces can <a href='https://spec.matrix.org/v1.17/client-server-api/#restricted-rooms'>join without an invitation</a>.")
                color: restrictedRowHover.hovered ? palette.brightText : palette.buttonText
                font.pointSize: 0.9 * Settings.uiFontSizePt
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingMedium
                Layout.rightMargin: Komai.paddingMedium
                Layout.bottomMargin: Komai.paddingMedium
                wrapMode: Text.Wrap
                textFormat: Text.RichText
                onLinkActivated: function(link) { Qt.openUrlExternally(link); }
            }
        }
    }

    // Rooms to join via
    Item {
        Layout.fillWidth: true
        Layout.leftMargin: Komai.paddingMedium
        Layout.rightMargin: Komai.paddingMedium
        implicitHeight: joinViaRowContent.implicitHeight
        visible: restrictedButton.checked && accessCombo.isPrivate
        HoverHandler { id: joinViaRowHover; blocking: false }
        Rectangle { anchors.fill: joinViaRowContent; color: joinViaRowHover.hovered ? palette.dark : palette.window; radius: Komai.paddingMedium; z: -1 }
        RowLayout {
            id: joinViaRowContent
            width: parent.width

            Label {
                text: qsTr("Rooms to join via")
                color: joinViaRowHover.hovered ? palette.brightText : palette.text
                font.pointSize: 1.1 * Settings.uiFontSizePt
                Layout.fillWidth: true
                Layout.topMargin: Komai.paddingMedium
                Layout.bottomMargin: Komai.paddingMedium
                Layout.leftMargin: Komai.paddingMedium
            }

            Components.KomaiButton {
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
        Layout.leftMargin: Komai.paddingMedium
        Layout.rightMargin: Komai.paddingMedium
        implicitHeight: guestRowContent.implicitHeight
        HoverHandler { id: guestRowHover; blocking: false }
        Rectangle { anchors.fill: guestRowContent; color: guestRowHover.hovered ? palette.dark : palette.window; radius: Komai.paddingMedium; z: -1 }
        ColumnLayout {
            id: guestRowContent
            width: parent.width
            spacing: 0

            RowLayout {
                Layout.fillWidth: true
                Layout.topMargin: Komai.paddingMedium
                Layout.leftMargin: Komai.paddingMedium
                Layout.rightMargin: Komai.paddingMedium

                Label {
                    text: qsTr("Allow guests to join")
                    color: guestRowHover.hovered ? palette.brightText : palette.text
                    font.pointSize: 1.1 * Settings.uiFontSizePt
                    Layout.fillWidth: true
                }

                ToggleButton {
                    id: guestAccessButton
                    textColor: guestRowHover.hovered ? palette.brightText : palette.buttonText
                    enabled: detailsGrid.roomSettings.canChangeJoinRules
                    checked: detailsGrid.roomSettings.guestAccess
                }
            }

            Label {
                text: qsTr("Lets <a href='https://spec.matrix.org/v1.17/client-server-api/#guest-access'>temporary accounts</a> without full registration join the room.")
                color: guestRowHover.hovered ? palette.brightText : palette.buttonText
                font.pointSize: 0.9 * Settings.uiFontSizePt
                Layout.fillWidth: true
                Layout.leftMargin: Komai.paddingMedium
                Layout.rightMargin: Komai.paddingMedium
                Layout.bottomMargin: Komai.paddingMedium
                wrapMode: Text.Wrap
                textFormat: Text.RichText
                onLinkActivated: function(link) { Qt.openUrlExternally(link); }
            }
        }
    }

    // Apply access rules button
    Components.KomaiButton {
        visible: accessCombo.isPrivate != detailsGrid.roomSettings.privateAccess || knockingButton.checked != detailsGrid.roomSettings.knockingEnabled || restrictedButton.checked != detailsGrid.roomSettings.restrictedEnabled || guestAccessButton.checked != detailsGrid.roomSettings.guestAccess || detailsGrid.roomSettings.allowedRoomsModified
        enabled: detailsGrid.roomSettings.canChangeJoinRules
        text: qsTr("Apply access rules")
        onClicked: detailsGrid.roomSettings.changeAccessRules(accessCombo.isPrivate, guestAccessButton.checked, knockingButton.checked, restrictedButton.checked)
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
        Layout.leftMargin: Komai.paddingMedium
        Layout.rightMargin: Komai.paddingMedium
        implicitHeight: publicHistoryRowContent.implicitHeight
        HoverHandler { id: publicHistoryRowHover; blocking: false }
        Rectangle { anchors.fill: publicHistoryRowContent; color: publicHistoryRowHover.hovered ? palette.dark : palette.window; radius: Komai.paddingMedium; z: -1 }
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
                    color: publicHistoryRowHover.hovered ? palette.brightText : palette.text
                    font.pointSize: 1.1 * Settings.uiFontSizePt
                    Layout.fillWidth: true
                }

                ToggleButton {
                    id: publicHistoryButton
                    textColor: publicHistoryRowHover.hovered ? palette.brightText : palette.buttonText
                    enabled: detailsGrid.roomSettings.canChangeHistoryVisibility
                    checked: detailsGrid.roomSettings.historyVisibility == RoomSettings.WorldReadable
                }
            }

            Label {
                text: qsTr("Useful for room previews and public websites.")
                color: publicHistoryRowHover.hovered ? palette.brightText : palette.buttonText
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
        Layout.leftMargin: Komai.paddingMedium
        Layout.rightMargin: Komai.paddingMedium
        implicitHeight: historyComboRowContent.implicitHeight
        visible: !publicHistoryButton.checked
        HoverHandler { id: historyComboRowHover; blocking: false }
        Rectangle { anchors.fill: historyComboRowContent; color: historyComboRowHover.hovered ? palette.dark : palette.window; radius: Komai.paddingMedium; z: -1 }
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
                    color: historyComboRowHover.hovered ? palette.brightText : palette.text
                    font.pointSize: 1.1 * Settings.uiFontSizePt
                    Layout.fillWidth: true
                }

                Components.KomaiComboBox {
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
                color: historyComboRowHover.hovered ? palette.brightText : palette.buttonText
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
    Components.KomaiButton {
        visible: !publicHistoryButton.checked && detailsGrid.roomSettings.historyVisibility !== historyCombo.visibilityValues[historyCombo.currentIndex]
        enabled: detailsGrid.roomSettings.canChangeHistoryVisibility
        text: qsTr("Apply visibility changes")
        property int selectedVisibility: publicHistoryButton.checked ? RoomSettings.WorldReadable : historyCombo.visibilityValues[historyCombo.currentIndex]
        onClicked: detailsGrid.roomSettings.changeHistoryVisibility(selectedVisibility)
        Layout.fillWidth: true
        Layout.leftMargin: Komai.paddingMedium
        Layout.rightMargin: Komai.paddingMedium
    }

    Components.KomaiButton {
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
        Layout.leftMargin: Komai.paddingMedium
        Layout.rightMargin: Komai.paddingMedium
        implicitHeight: hiddenEventsRowContent.implicitHeight
        HoverHandler { id: hiddenEventsRowHover; blocking: false }
        Rectangle { anchors.fill: hiddenEventsRowContent; color: hiddenEventsRowHover.hovered ? palette.dark : palette.window; radius: Komai.paddingMedium; z: -1 }
        RowLayout {
            id: hiddenEventsRowContent
            width: parent.width

            Label {
                text: qsTr("Locally hidden events")
                color: hiddenEventsRowHover.hovered ? palette.brightText : palette.text
                font.pointSize: 1.1 * Settings.uiFontSizePt
                Layout.topMargin: Komai.paddingMedium
                Layout.bottomMargin: Komai.paddingMedium
                Layout.leftMargin: Komai.paddingMedium
            }

            Item { Layout.fillWidth: true }

            HiddenEventsDialog {
                id: hiddenEventsDialog
                roomid: detailsGrid.roomSettings ? detailsGrid.roomSettings.roomId : ""
            }

            Components.KomaiButton {
                text: qsTr("Configure")
                onClicked: hiddenEventsDialog.open()
                Layout.rightMargin: Komai.paddingMedium
            }
        }
    }

    // Event expiration
    Item {
        Layout.fillWidth: true
        Layout.leftMargin: Komai.paddingMedium
        Layout.rightMargin: Komai.paddingMedium
        implicitHeight: eventExpRowContent.implicitHeight
        HoverHandler { id: eventExpRowHover; blocking: false }
        Rectangle { anchors.fill: eventExpRowContent; color: eventExpRowHover.hovered ? palette.dark : palette.window; radius: Komai.paddingMedium; z: -1 }
        RowLayout {
            id: eventExpRowContent
            width: parent.width

            Label {
                text: qsTr("Event expiration")
                color: eventExpRowHover.hovered ? palette.brightText : palette.text
                font.pointSize: 1.1 * Settings.uiFontSizePt
                Layout.topMargin: Komai.paddingMedium
                Layout.bottomMargin: Komai.paddingMedium
                Layout.leftMargin: Komai.paddingMedium
            }

            Components.SyncedToMatrixBadge {
                Layout.alignment: Qt.AlignVCenter
            }

            Item { Layout.fillWidth: true }

            EventExpirationDialog {
                id: eventExpirationDialog
                roomid: detailsGrid.roomSettings ? detailsGrid.roomSettings.roomId : ""
                roomName: detailsGrid.roomSettings ? detailsGrid.roomSettings.roomName : ""
            }

            Components.KomaiButton {
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
        Layout.leftMargin: Komai.paddingMedium
        Layout.rightMargin: Komai.paddingMedium
        implicitHeight: stickerRowContent.implicitHeight
        HoverHandler { id: stickerRowHover; blocking: false }
        Rectangle { anchors.fill: stickerRowContent; color: stickerRowHover.hovered ? palette.dark : palette.window; radius: Komai.paddingMedium; z: -1 }
        RowLayout {
            id: stickerRowContent
            width: parent.width

            Label {
                text: qsTr("Sticker & Emote Settings")
                color: stickerRowHover.hovered ? palette.brightText : palette.text
                font.pointSize: 1.1 * Settings.uiFontSizePt
                Layout.topMargin: Komai.paddingMedium
                Layout.bottomMargin: Komai.paddingMedium
                Layout.leftMargin: Komai.paddingMedium
            }

            Components.SyncedToMatrixBadge {
                Layout.alignment: Qt.AlignVCenter
            }

            Item { Layout.fillWidth: true }

            Components.KomaiButton {
                text: qsTr("Change")
                onClicked: TimelineManager.openImagePackSettings(detailsGrid.roomSettings.roomId)
                Layout.rightMargin: Komai.paddingMedium
            }
        }
    }
}
