// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../../components"
import "../../../ui"
import QtQuick 2.15
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.2
import cc.etke.komai 1.0

ColumnLayout {
    required property var roomSettings
    required property real dialogWidth

    Layout.fillWidth: true
    Layout.preferredWidth: dialogWidth
    Layout.minimumWidth: dialogWidth
    Layout.maximumWidth: dialogWidth

    Avatar {
        id: displayAvatar

        Layout.topMargin: Komai.paddingMedium
        url: roomSettings.roomAvatarUrl.replace("mxc://", "image://MxcImage/")
        roomid: roomSettings.roomId
        displayName: roomSettings.roomName
        Layout.preferredHeight: 130
        Layout.preferredWidth: 130
        Layout.alignment: Qt.AlignHCenter
        onClicked: TimelineManager.openImageOverlay(null, roomSettings.roomAvatarUrl, "", 0, 0)

        ImageButton {
            hoverEnabled: true
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Change room avatar.")
            anchors.left: displayAvatar.left
            anchors.top: displayAvatar.top
            anchors.leftMargin: Komai.paddingMedium
            anchors.topMargin: Komai.paddingMedium
            visible: roomSettings.canChangeAvatar
            image: ":/icons/icons/ui/edit.svg"

            onClicked: roomSettings.updateAvatar()
        }
    }

    Spinner {
        Layout.alignment: Qt.AlignHCenter
        visible: roomSettings.isLoading
        foreground: palette.mid
        running: roomSettings.isLoading
    }

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

    TextEdit {
        id: roomName

        property bool isNameEditingAllowed: false

        readOnly: !isNameEditingAllowed
        textFormat: isNameEditingAllowed ? TextEdit.PlainText : TextEdit.RichText
        text: isNameEditingAllowed ? roomSettings.plainRoomName : roomSettings.roomName
        font.pixelSize: fontMetrics.font.pixelSize * 2
        color: palette.text

        Layout.alignment: Qt.AlignHCenter
        Layout.maximumWidth: Math.max(0, dialogWidth - (Komai.paddingSmall + roomNameButtons.anchors.leftMargin + roomNameButtons.implicitWidth) * 2)
        horizontalAlignment: TextEdit.AlignHCenter
        wrapMode: TextEdit.Wrap
        selectByMouse: true

        Keys.onShortcutOverride: event.key === Qt.Key_Enter
        Keys.onPressed: {
            if (event.matches(StandardKey.InsertLineSeparator) || event.matches(StandardKey.InsertParagraphSeparator)) {
                roomSettings.changeName(roomName.text);
                roomName.isNameEditingAllowed = false;
                event.accepted = true;
            }
        }

        RowLayout {
            id: roomNameButtons

            anchors.leftMargin: Komai.paddingSmall
            anchors.left: roomName.right
            anchors.verticalCenter: roomName.verticalCenter

            ImageButton {
                id: nameChangeButton

                visible: roomSettings.canChangeName
                hoverEnabled: true
                ToolTip.visible: hovered
                ToolTip.text: qsTr("Change name of this room")
                ToolTip.delay: Komai.tooltipDelay
                image: roomName.isNameEditingAllowed ? ":/icons/icons/ui/checkmark.svg" : ":/icons/icons/ui/edit.svg"

                onClicked: {
                    if (roomName.isNameEditingAllowed) {
                        roomSettings.changeName(roomName.text);
                        roomName.isNameEditingAllowed = false;
                    } else {
                        roomName.isNameEditingAllowed = true;
                        roomName.focus = true;
                        roomName.selectAll();
                    }
                }
            }

            EncryptionIndicator {
                Layout.preferredHeight: 16
                Layout.preferredWidth: 16
                sourceSize.width: width
                sourceSize.height: height
                encrypted: true
                visible: roomSettings.isEncryptionEnabled && (roomSettings.isRoomNameSet || !roomName.readOnly)
                trust: Crypto.Unverified
                ToolTip.text: qsTr("Since room state can't be encrypted, make sure no confidential information is stored in the room name!")
            }
        }
    }

    Label {
        text: qsTr("%n member(s)", "", roomSettings.memberCount)
        color: palette.text
        Layout.alignment: Qt.AlignHCenter
    }
}
