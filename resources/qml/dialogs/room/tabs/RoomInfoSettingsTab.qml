// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../components"
import "../../../components" as Components
import "../../../ui"
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import cc.etke.komai 1.0

Item {
    id: settingsTab

    property var roomSettings
    property var members
    property var room
    property var appRoot

    ScrollView {
        id: scrollView

        anchors.fill: parent
        ScrollBar.vertical.policy: ScrollBar.AlwaysOn
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            id: scrollContent

            width: scrollView.availableWidth
            spacing: 0

            // Loading spinner
            Spinner {
                Layout.alignment: Qt.AlignHCenter
                visible: roomSettings && roomSettings.isLoading
                foreground: palette.mid
                running: roomSettings && roomSettings.isLoading
            }

            // Error text with fade animation
            Text {
                id: errorText

                color: "red"
                visible: opacity > 0
                opacity: 0
                Layout.alignment: Qt.AlignHCenter
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }

            SequentialAnimation {
                id: hideErrorAnimation

                running: false

                PauseAnimation {
                    duration: 4000
                }

                NumberAnimation {
                    target: errorText
                    property: "opacity"
                    to: 0
                    duration: 1000
                }
            }

            Connections {
                target: roomSettings ?? null

                function onDisplayError(errorMessage) {
                    errorText.text = errorMessage;
                    errorText.opacity = 1;
                    hideErrorAnimation.restart();
                }
            }

            // --- General settings section ---
            Components.SettingsSection {
                label: qsTr("General settings")
                Layout.fillWidth: true
                Layout.topMargin: Komai.paddingMedium
                Layout.leftMargin: Komai.paddingMedium
                Layout.rightMargin: Komai.paddingMedium
            }

            // Avatar row
            Item {
                Layout.fillWidth: true
                implicitHeight: avatarRowContent.implicitHeight

                HoverHandler { id: avatarRowHover; blocking: false }
                Rectangle {
                    anchors.fill: avatarRowContent
                    color: palette.alternateBase
                    radius: Komai.paddingMedium
                    visible: avatarRowHover.hovered
                    z: -1
                }

                ColumnLayout {
                    id: avatarRowContent
                    width: parent.width
                    spacing: 0

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: Komai.paddingMedium
                        Layout.bottomMargin: Komai.paddingMedium
                        Layout.leftMargin: Komai.paddingMedium
                        Layout.rightMargin: Komai.paddingMedium

                        Label {
                            text: qsTr("Avatar")
                            color: palette.text
                            font.pointSize: 1.1 * Settings.uiFontSizePt
                            Layout.fillWidth: true
                        }

                        Button {
                            text: qsTr("Change")
                            icon.source: "qrc:/icons/icons/ui/edit.svg"
                            visible: roomSettings && roomSettings.canChangeAvatar
                            onClicked: roomSettings.updateAvatar()
                        }

                        Button {
                            text: qsTr("Remove")
                            icon.source: "qrc:/icons/icons/ui/delete.svg"
                            visible: roomSettings && roomSettings.canChangeAvatar && roomSettings.roomAvatarUrl !== ""
                            onClicked: confirmRemoveAvatarDialog.open()
                        }

                        Avatar {
                            id: displayAvatar

                            url: roomSettings ? roomSettings.roomAvatarUrl.replace("mxc://", "image://MxcImage/") : ""
                            roomid: roomSettings ? roomSettings.roomId : ""
                            displayName: roomSettings ? roomSettings.roomName : ""
                            Layout.preferredHeight: 72
                            Layout.preferredWidth: 72
                            enabled: false
                        }
                    }
                }

                Components.OverlayDialog {
                    id: confirmRemoveAvatarDialog

                    title: qsTr("Remove avatar")
                    titleIcon: ":/icons/icons/ui/delete.svg"

                    Label {
                        Layout.fillWidth: true
                        color: palette.text
                        wrapMode: Text.WordWrap
                        text: qsTr("Are you sure you want to remove the room avatar?")
                    }

                    Button {
                        Layout.alignment: Qt.AlignRight
                        text: qsTr("Remove")
                        highlighted: true
                        onClicked: {
                            roomSettings.removeAvatar();
                            confirmRemoveAvatarDialog.close();
                        }
                    }
                }
            }

            // Name row
            Item {
                Layout.fillWidth: true
                implicitHeight: nameRowContent.implicitHeight

                HoverHandler { id: nameRowHover; blocking: false }
                Rectangle {
                    anchors.fill: nameRowContent
                    color: palette.alternateBase
                    radius: Komai.paddingMedium
                    visible: nameRowHover.hovered
                    z: -1
                }

                ColumnLayout {
                    id: nameRowContent
                    width: parent.width
                    spacing: 0

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: Komai.paddingMedium
                        Layout.bottomMargin: Komai.paddingMedium
                        Layout.leftMargin: Komai.paddingMedium
                        Layout.rightMargin: Komai.paddingMedium

                        Label {
                            text: qsTr("Name")
                            color: palette.text
                            font.pointSize: 1.1 * Settings.uiFontSizePt
                            Layout.fillWidth: true
                        }

                        TextField {
                            id: roomNameField

                            property string lastSubmitted: ""
                            property string serverValue: roomSettings ? roomSettings.plainRoomName : ""
                            onServerValueChanged: lastSubmitted = ""

                            text: serverValue
                            readOnly: !(roomSettings && roomSettings.canChangeName)
                            Layout.preferredWidth: scrollView.availableWidth * 0.5

                            function applyName() {
                                var val = text.trim();
                                if (roomSettings && val !== roomSettings.plainRoomName && val !== lastSubmitted) {
                                    lastSubmitted = val;
                                    roomSettings.changeName(val);
                                }
                            }

                            onEditingFinished: applyName()
                            onActiveFocusChanged: if (!activeFocus) applyName()
                            Component.onDestruction: applyName()
                        }
                    }
                }
            }

            // Topic row
            Item {
                Layout.fillWidth: true
                implicitHeight: topicRowContent.implicitHeight

                HoverHandler { id: topicRowHover; blocking: false }
                Rectangle {
                    anchors.fill: topicRowContent
                    color: palette.alternateBase
                    radius: Komai.paddingMedium
                    visible: topicRowHover.hovered
                    z: -1
                }

                ColumnLayout {
                    id: topicRowContent
                    width: parent.width
                    spacing: 0

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: Komai.paddingMedium
                        Layout.bottomMargin: Komai.paddingMedium
                        Layout.leftMargin: Komai.paddingMedium
                        Layout.rightMargin: Komai.paddingMedium

                        Label {
                            text: qsTr("Topic")
                            color: palette.text
                            font.pointSize: 1.1 * Settings.uiFontSizePt
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignTop
                        }

                        ScrollView {
                            Layout.preferredWidth: scrollView.availableWidth * 0.5
                            Layout.minimumHeight: roomTopicField.font.pixelSize * 4

                            TextArea {
                                id: roomTopicField

                                property string lastSubmitted: ""
                                property string serverValue: roomSettings ? roomSettings.plainRoomTopic : ""
                                onServerValueChanged: lastSubmitted = ""

                                text: serverValue
                                readOnly: !(roomSettings && roomSettings.canChangeTopic)
                                placeholderText: qsTr("No topic set")
                                wrapMode: TextEdit.WordWrap
                                color: palette.text

                                function applyTopic() {
                                    var val = text.trim();
                                    if (roomSettings && val !== roomSettings.plainRoomTopic && val !== lastSubmitted) {
                                        lastSubmitted = val;
                                        roomSettings.changeTopic(val);
                                    }
                                }

                                onEditingFinished: applyTopic()
                                onActiveFocusChanged: if (!activeFocus) applyTopic()
                                Component.onDestruction: applyTopic()
                            }
                        }
                    }
                }
            }

            // Encryption row
            Item {
                Layout.fillWidth: true
                implicitHeight: encryptionRowContent.implicitHeight

                HoverHandler { id: encryptionRowHover; blocking: false }
                Rectangle {
                    anchors.fill: encryptionRowContent
                    color: palette.alternateBase
                    radius: Komai.paddingMedium
                    visible: encryptionRowHover.hovered
                    z: -1
                }

                ColumnLayout {
                    id: encryptionRowContent
                    width: parent.width
                    spacing: 0

                    RowLayout {
                        Layout.fillWidth: true
                        Layout.topMargin: Komai.paddingMedium
                        Layout.leftMargin: Komai.paddingMedium
                        Layout.rightMargin: Komai.paddingMedium

                        Label {
                            text: qsTr("Encryption")
                            color: palette.text
                            font.pointSize: 1.1 * Settings.uiFontSizePt
                            Layout.fillWidth: true
                        }

                        ToggleButton {
                            id: encryptionToggle

                            checked: roomSettings ? roomSettings.isEncryptionEnabled : false
                            onCheckedChanged: {
                                if (roomSettings && roomSettings.isEncryptionEnabled) {
                                    checked = true;
                                    return;
                                }
                                if (checked === true)
                                    confirmEncryptionDialog.open();
                            }
                        }
                    }

                    Label {
                        text: qsTr("Once enabled, encryption cannot be disabled.")
                        color: palette.buttonText
                        font.pointSize: 0.9 * Settings.uiFontSizePt
                        Layout.fillWidth: true
                        Layout.leftMargin: Komai.paddingMedium
                        Layout.rightMargin: Komai.paddingMedium
                        Layout.bottomMargin: Komai.paddingMedium
                        wrapMode: Text.Wrap
                    }
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
                            if (roomSettings && !roomSettings.isEncryptionEnabled)
                                roomSettings.enableEncryption();
                            confirmEncryptionDialog.wasAccepted = true;
                            confirmEncryptionDialog.close();
                        }
                    }
                }
            }

            RoomSettingsDetailsGrid {
                roomSettings: settingsTab.roomSettings
                appRoot: settingsTab.appRoot
            }
        }
    }
}
