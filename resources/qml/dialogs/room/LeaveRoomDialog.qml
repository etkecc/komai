// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components" as Components
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai 1.0

Components.OverlayDialog {
    id: leaveRoomRoot

    required property string roomId
    property string reason: ""
    readonly property var roomPreview: Rooms.getRoomPreviewById(roomId)
    readonly property bool isSpace: !!roomPreview && roomPreview.isSpace
    readonly property bool hasVisibilityInfo: !!roomPreview && roomPreview.roomid !== ""
    readonly property bool isPublic: roomPreview ? roomPreview.isPublic : false
    readonly property string roomName: roomPreview ? roomPreview.roomName : ""
    readonly property color leaveHintColor: isPublic ? Komai.theme.success : Komai.theme.attention
    readonly property string leaveHintText: {
        if (isPublic) {
            return isSpace ? qsTr("This is a public space, so re-joining later should be easy.")
                           : qsTr("This is a public room, so re-joining later should be easy.");
        }

        return isSpace ? qsTr("This is a private space, so you may need an invitation to re-join.")
                       : qsTr("This is a private room, so you may need an invitation to re-join.");
    }

    title: {
        if (roomName) {
            return isSpace
                ? qsTr("Leave the %1 space?").arg(roomName)
                : qsTr("Leave the %1 room?").arg(roomName);
        }
        return isSpace ? qsTr("Leave this space?") : qsTr("Leave this room?");
    }
    titleIcon: ":/icons/icons/ui/power-off.svg"

    function submit() {
        if (CallManager.haveCallInvite) {
            CallManager.rejectInvite();
        } else if (CallManager.isOnCall) {
            CallManager.hangUp();
        }
        Rooms.leave(leaveRoomRoot.roomId, reasonInput.text);
        leaveRoomRoot.close();
    }

    RowLayout {
        Layout.fillWidth: true
        visible: leaveRoomRoot.hasVisibilityInfo
        spacing: Komai.paddingMedium

        Image {
            Layout.alignment: Qt.AlignTop
            Layout.preferredHeight: 20
            Layout.preferredWidth: 20
            fillMode: Image.PreserveAspectFit
            source: (leaveRoomRoot.isPublic
                    ? "image://colorimage/:/icons/icons/ui/people-community.svg?"
                    : "image://colorimage/:/icons/icons/ui/lock-closed.svg?")
                + leaveRoomRoot.leaveHintColor
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: Komai.paddingSmall

            Label {
                Layout.fillWidth: true
                color: leaveRoomRoot.leaveHintColor
                text: leaveRoomRoot.leaveHintText
                wrapMode: Text.WordWrap
            }

            Label {
                Layout.fillWidth: true
                color: palette.text
                text: qsTr("You will remain in any rooms you joined through it.")
                visible: leaveRoomRoot.isSpace
                wrapMode: Text.WordWrap
            }
        }
    }

    Label {
        Layout.fillWidth: true
        color: palette.buttonText
        wrapMode: Text.WordWrap
        text: qsTr("Re-joining may require an invitation depending on its join rules.")
        visible: !leaveRoomRoot.hasVisibilityInfo
    }

    Components.KomaiTextField {
        id: reasonInput

        Layout.fillWidth: true
        text: leaveRoomRoot.reason
        placeholderText: qsTr("Add optional reason for leaving")
        onAccepted: leaveRoomRoot.submit()

        Component.onCompleted: Qt.callLater(() => reasonInput.forceActiveFocus())
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Komai.paddingMedium

        Components.KomaiButton {
            text: qsTr("Cancel")
            onClicked: leaveRoomRoot.close()
        }

        Item {
            Layout.fillWidth: true
        }

        Components.KomaiButton {
            text: qsTr("Leave")
            highlighted: true
            onClicked: leaveRoomRoot.submit()
        }
    }
}
