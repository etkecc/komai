// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components"
import "../../ui"
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.2
import cc.etke.komai 1.0

OverlayDialog {
    id: joinRoomRoot

    required property RoomSummary summary

    title: summary.isSpace ? qsTr("Confirm community join") : qsTr("Confirm room join")
    titleIcon: ":/icons/icons/ui/plus-circle.svg"
    initialFocusItem: reason

    Avatar {
        Layout.topMargin: Komai.paddingMedium
        url: summary.roomAvatarUrl.replace("mxc://", "image://MxcImage/")
        roomid: summary.roomid
        displayName: summary.roomName
        Layout.preferredHeight: Komai.listIconSize
        Layout.preferredWidth: Komai.listIconSize
        Layout.alignment: Qt.AlignHCenter
    }

    Spinner {
        Layout.alignment: Qt.AlignHCenter
        visible: !summary.isLoaded
        foreground: palette.mid
        running: !summary.isLoaded
    }

    TextEdit {
        readOnly: true
        textFormat: TextEdit.RichText
        text: summary.roomName
        font.pixelSize: fontMetrics.font.pixelSize * 2
        color: palette.text

        Layout.alignment: Qt.AlignHCenter
        Layout.fillWidth: true
        horizontalAlignment: TextEdit.AlignHCenter
        wrapMode: TextEdit.Wrap
        selectByMouse: true
    }

    TextEdit {
        readOnly: true
        textFormat: TextEdit.PlainText
        text: summary.roomid
        font.pixelSize: fontMetrics.font.pixelSize * 0.8
        color: palette.text

        Layout.alignment: Qt.AlignHCenter
        Layout.fillWidth: true
        horizontalAlignment: TextEdit.AlignHCenter
        wrapMode: TextEdit.Wrap
        selectByMouse: true
    }

    RowLayout {
        spacing: Komai.paddingMedium
        Layout.alignment: Qt.AlignHCenter

        MatrixText {
            text: qsTr("%n member(s)", "", summary.memberCount)
        }

        ImageButton {
            image: ":/icons/icons/ui/people.svg"
            enabled: false
        }
    }

    TextEdit {
        readOnly: true
        textFormat: TextEdit.RichText
        text: summary.roomTopic
        color: palette.text

        Layout.alignment: Qt.AlignHCenter
        Layout.fillWidth: true
        horizontalAlignment: TextEdit.AlignHCenter
        wrapMode: TextEdit.Wrap
        selectByMouse: true
    }

    Label {
        text: summary.isKnockOnly ? qsTr("This room can't be joined directly. You can, however, knock on the room and room members can accept or decline this join request. You can additionally provide a reason for them to let you in below:") : qsTr("Do you want to join this room? You can optionally add a reason below:")
        color: palette.text
        Layout.fillWidth: true
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        font.bold: true
    }

    KomaiTextField {
        id: reason

        Layout.fillWidth: true
        text: joinRoomRoot.summary.reason
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Komai.paddingMedium

        KomaiButton {
            text: qsTr("Cancel")
            onClicked: joinRoomRoot.close()
        }

        Item {
            Layout.fillWidth: true
        }

        KomaiButton {
            text: summary.isKnockOnly ? qsTr("Knock") : qsTr("Join")
            highlighted: true
            onClicked: {
                summary.reason = reason.text;
                summary.join();
                joinRoomRoot.close();
            }
        }
    }
}
