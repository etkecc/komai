// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../moderation"
import "../../../ui"
import QtQuick 2.15
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.2
import "../../../components" as Components
import cc.etke.komai 1.0

GridLayout {
    required property var roomSettings
    required property var appRoot
                columns: 2
                rowSpacing: Komai.paddingMedium
                Layout.margins: Komai.paddingMedium
                Layout.fillWidth: true

                Label {
                    text: qsTr("NOTIFICATIONS")
                    font.bold: true
                    color: palette.text
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    Layout.topMargin: Komai.paddingLarge
                }

                Label {
                    text: qsTr("Notifications")
                    Layout.fillWidth: true
                    color: palette.text
                }

                ComboBox {
                    id: notificationsCombo
                    Layout.fillWidth: true
                    model: [qsTr("Muted"), qsTr("Mentions only"), qsTr("All messages")]
                    currentIndex: roomSettings.notifications
                    onActivated: (index) => {
                        roomSettings.changeNotifications(index);
                    }

                    // Disable built-in wheel handling unless focused
                    wheelEnabled: activeFocus
                }

                Label {
                    text: qsTr("ENTRY PERMISSIONS")
                    font.bold: true
                    color: palette.text
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    Layout.topMargin: Komai.paddingLarge
                }

                Label {
                    text: qsTr("Anyone can join")
                    Layout.fillWidth: true
                    color: palette.text
                }

                ToggleButton {
                    id: publicRoomButton

                    enabled: roomSettings.canChangeJoinRules
                    checked: !roomSettings.privateAccess
                    Layout.alignment: Qt.AlignRight
                }

                Label {
                    text: qsTr("Allow knocking")
                    Layout.fillWidth: true
                    color: palette.text
                    visible: knockingButton.visible
                }

                ToggleButton {
                    id: knockingButton

                    visible: !publicRoomButton.checked
                    enabled: roomSettings.canChangeJoinRules && roomSettings.supportsKnocking
                    checked: roomSettings.knockingEnabled
                    onCheckedChanged: {
                        if (checked && !roomSettings.supportsKnockRestricted) restrictedButton.checked = false;
                    }
                    Layout.alignment: Qt.AlignRight
                }

                Label {
                    text: qsTr("Allow joining via other rooms")
                    Layout.fillWidth: true
                    color: palette.text
                    visible: restrictedButton.visible
                }

                ToggleButton {
                    id: restrictedButton

                    visible: !publicRoomButton.checked
                    enabled: roomSettings.canChangeJoinRules && roomSettings.supportsRestricted
                    checked: roomSettings.restrictedEnabled
                    onCheckedChanged: {
                        if (checked && !roomSettings.supportsKnockRestricted) knockingButton.checked = false;
                    }
                    Layout.alignment: Qt.AlignRight
                }

                Label {
                    text: qsTr("Rooms to join via")
                    Layout.fillWidth: true
                    color: palette.text
                    visible: allowedRoomsButton.visible
                }

                Button {
                    id: allowedRoomsButton

                    visible: restrictedButton.checked && restrictedButton.visible
                    enabled: roomSettings.canChangeJoinRules && roomSettings.supportsRestricted

                    text: qsTr("Change")
                    ToolTip.text: qsTr("Change the list of rooms users can join this room via. Usually this is the official community of this room.")
                    onClicked: appRoot.showAllowedRoomsEditor(roomSettings)
                    Layout.alignment: Qt.AlignRight
                }

                Label {
                    text: qsTr("Allow guests to join")
                    Layout.fillWidth: true
                    color: palette.text
                }

                ToggleButton {
                    id: guestAccessButton

                    enabled: roomSettings.canChangeJoinRules
                    checked: roomSettings.guestAccess
                    Layout.alignment: Qt.AlignRight
                }

                Button {
                    visible: publicRoomButton.checked == roomSettings.privateAccess || knockingButton.checked != roomSettings.knockingEnabled || restrictedButton.checked != roomSettings.restrictedEnabled || guestAccessButton.checked != roomSettings.guestAccess || roomSettings.allowedRoomsModified
                    enabled: roomSettings.canChangeJoinRules

                    text: qsTr("Apply access rules")
                    onClicked: roomSettings.changeAccessRules(!publicRoomButton.checked, guestAccessButton.checked, knockingButton.checked, restrictedButton.checked)
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                }

                Label {
                    text: qsTr("MESSAGE VISIBILITY")
                    font.bold: true
                    color: palette.text
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    Layout.topMargin: Komai.paddingLarge
                }

                Label {
                    text: qsTr("Allow viewing history without joining")
                    Layout.fillWidth: true
                    color: palette.text
                    ToolTip.text: qsTr("This is useful to see previews of the room or view it on public websites.")
                    ToolTip.visible: publicHistoryHover.hovered
                    ToolTip.delay: Komai.tooltipDelay

                    HoverHandler {
                        id: publicHistoryHover

                    }
                }

                ToggleButton {
                    id: publicHistoryButton

                    enabled: roomSettings.canChangeHistoryVisibility
                    checked: roomSettings.historyVisibility == RoomSettings.WorldReadable
                    Layout.alignment: Qt.AlignRight
                }

                Label {
                    visible: !publicHistoryButton.checked
                    text: qsTr("Members can see messages since")
                    Layout.fillWidth: true
                    color: palette.text
                    Layout.alignment: Qt.AlignTop | Qt.AlignLeft
                    ToolTip.text: qsTr("How much of the history is visible to joined members. Changing this won't affect the visibility of already sent messages. It only applies to new messages.")
                    ToolTip.visible: privateHistoryHover.hovered
                    ToolTip.delay: Komai.tooltipDelay

                    HoverHandler {
                        id: privateHistoryHover

                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    visible: !publicHistoryButton.checked
                    enabled: roomSettings.canChangeHistoryVisibility
                    Layout.alignment: Qt.AlignTop | Qt.AlignRight

                    RadioButton {
                        id: sharedHistory
                        checked: roomSettings.historyVisibility == RoomSettings.Shared
                        text: qsTr("Everything")
                        ToolTip.text: qsTr("As long as the user joined, they can see all previous messages.")
                        ToolTip.visible: hovered
                        ToolTip.delay: Komai.tooltipDelay
                    }
                    RadioButton {
                        id: invitedHistory
                        checked: roomSettings.historyVisibility == RoomSettings.Invited
                        text: qsTr("They got invited")
                        ToolTip.text: qsTr("Members can only see messages from when they got invited going forward.")
                        ToolTip.visible: hovered
                        ToolTip.delay: Komai.tooltipDelay
                    }
                    RadioButton {
                        id: joinedHistory
                        checked: roomSettings.historyVisibility == RoomSettings.Joined || roomSettings.historyVisibility == RoomSettings.WorldReadable
                        text: qsTr("They joined")
                        ToolTip.text: qsTr("Members can only see messages since after they joined.")
                        ToolTip.visible: hovered
                        ToolTip.delay: Komai.tooltipDelay
                    }
                }

                Button {
                    visible: roomSettings.historyVisibility != selectedVisibility
                    enabled: roomSettings.canChangeHistoryVisibility

                    text: qsTr("Apply visibility changes")
                    property int selectedVisibility: {
                        if (publicHistoryButton.checked)
                            return RoomSettings.WorldReadable;
                        else if (sharedHistory.checked)
                            return RoomSettings.Shared;
                        else if (invitedHistory.checked)
                            return RoomSettings.Invited;
                        return RoomSettings.Joined;
                    }
                    onClicked: roomSettings.changeHistoryVisibility(selectedVisibility)
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                }

                Label {
                    text: qsTr("Locally hidden events")
                    color: palette.text
                }

                HiddenEventsDialog {
                    id: hiddenEventsDialog
                    roomid: roomSettings.roomId
                    roomName: roomSettings.roomName
                }

                Button {
                    text: qsTr("Configure")
                    ToolTip.text: qsTr("Select events to hide in this room")
                    onClicked: hiddenEventsDialog.open()
                    Layout.alignment: Qt.AlignRight
                }

                Label {
                    text: qsTr("Automatic event deletion")
                    color: palette.text
                }

                EventExpirationDialog {
                    id: eventExpirationDialog
                    roomid: roomSettings.roomId
                    roomName: roomSettings.roomName
                }

                Button {
                    text: qsTr("Configure")
                    ToolTip.text: qsTr("Select if your events get automatically deleted in this room.")
                    onClicked: eventExpirationDialog.open()
                    Layout.alignment: Qt.AlignRight
                }

                Label {
                    text: qsTr("GENERAL SETTINGS")
                    font.bold: true
                    color: palette.text
                    Layout.columnSpan: 2
                    Layout.fillWidth: true
                    Layout.topMargin: Komai.paddingLarge
                }

                Label {
                    text: qsTr("Encryption")
                    color: palette.text
                }

                ToggleButton {
                    id: encryptionToggle

                    checked: roomSettings.isEncryptionEnabled
                    onCheckedChanged: {
                        if (roomSettings.isEncryptionEnabled) {
                            checked = true;
                            return ;
                        }
                        if (checked === true)
                            confirmEncryptionDialog.open();
                    }
                    Layout.alignment: Qt.AlignRight
                }

                Components.OverlayDialog {
                    id: confirmEncryptionDialog

                    property bool wasAccepted: false

                    title: qsTr("End-to-End Encryption")
                    titleIcon: ":/icons/icons/ui/shield-regular.svg"

                    onOpened: wasAccepted = false
                    onClosed: {
                        if (!wasAccepted)
                            encryptionToggle.checked = false;
                    }

                    Label {
                        Layout.fillWidth: true
                        color: palette.text
                        wrapMode: Text.WordWrap
                        text: qsTr("Encryption is currently experimental and things might break unexpectedly.\nPlease take note that it can't be disabled afterwards.")
                    }

                    Button {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("Enable")
                        highlighted: true
                        onClicked: {
                            if (!roomSettings.isEncryptionEnabled)
                                roomSettings.enableEncryption();
                            confirmEncryptionDialog.wasAccepted = true;
                            confirmEncryptionDialog.close();
                        }
                    }
                }

                Label {
                    text: qsTr("Permission")
                    color: palette.text
                }

                Button {
                    text: qsTr("Configure")
                    ToolTip.text: qsTr("View and change the permissions in this room")
                    onClicked: appRoot.showPLEditor(roomSettings)
                    Layout.alignment: Qt.AlignRight
                }

                Label {
                    text: qsTr("Aliases")
                    color: palette.text
                }

                Button {
                    text: qsTr("Configure")
                    ToolTip.text: qsTr("View and change the addresses/aliases of this room")
                    onClicked: appRoot.showAliasEditor(roomSettings)
                    Layout.alignment: Qt.AlignRight
                }

                Label {
                    text: qsTr("Sticker & Emote Settings")
                    color: palette.text
                }

                Button {
                    text: qsTr("Change")
                    ToolTip.text: qsTr("Change what packs are enabled, remove packs, or create new ones")
                    onClicked: TimelineManager.openImagePackSettings(roomSettings.roomId)
                    Layout.alignment: Qt.AlignRight
                }

                Label {
                    text: qsTr("INFO")
                    font.bold: true
                    color: palette.text
                    Layout.columnSpan: 2
                    Layout.topMargin: Komai.paddingLarge
                    Layout.fillWidth: true
                }

                Label {
                    text: qsTr("Internal ID")
                    color: palette.text
                }

                AbstractButton { // AbstractButton does not allow setting text color
                    Layout.alignment: Qt.AlignRight
                    Layout.fillWidth: true
                    Layout.preferredHeight: idLabel.height
                    Label { // TextEdit does not trigger onClicked
                        id: idLabel
                        text: roomSettings.roomId
                        textFormat: Text.PlainText
                        font.pixelSize: Math.floor(fontMetrics.font.pixelSize * 0.8)
                        color: palette.text
                        width: parent.width
                        horizontalAlignment: Text.AlignRight
                        wrapMode: Text.WrapAnywhere
                        ToolTip.text: qsTr("Copied to clipboard")
                        ToolTip.visible: toolTipTimer.running
                    }
                    TextEdit{ // label does not allow selection
                        id: textEdit
                        textFormat: TextEdit.PlainText
                        visible: false
                        text: roomSettings.roomId
                    }
                    onClicked: {
                        textEdit.selectAll()
                        textEdit.copy()
                        toolTipTimer.start()
                    }
                    Timer {
                        id: toolTipTimer
                    }
                }

                Label {
                    text: qsTr("Room Version")
                    color: palette.text
                }

                Label {
                    text: roomSettings.roomVersion
                    font.pixelSize: fontMetrics.font.pixelSize
                    Layout.alignment: Qt.AlignRight
                    color: palette.text
                }

            }
