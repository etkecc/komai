// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai

ColumnLayout {
    id: root

    required property var dialogHost

    spacing: 0

    Image {
        Layout.alignment: Qt.AlignHCenter
        Layout.preferredWidth: Komai.timelineLogoSize
        Layout.preferredHeight: Komai.timelineLogoSize
        source: "qrc:/logos/komai.svg"
        sourceSize.height: Komai.timelineLogoSize * 2
        sourceSize.width: Komai.timelineLogoSize * 2
        fillMode: Image.PreserveAspectFit
    }

    Label {
        Layout.alignment: Qt.AlignHCenter
        Layout.topMargin: Komai.paddingMedium
        Layout.bottomMargin: Komai.paddingLarge
        font.pointSize: Settings.uiFontSizePt * 1.4
        color: palette.buttonText
        text: {
            const messages = [
                qsTr("The ten thousand chats can't happen in a void. Open a room?"),
                qsTr("Your friends are just a room away"),
                qsTr("Connect with friends. Or bots. We don't judge."),
                qsTr("Friends, bots, communities - all one click away"),
                qsTr("Be present for a bit. Then open a room."),
                qsTr("The best conversations haven't happened yet"),
                qsTr("An empty screen, a full inbox of possibilities"),
                qsTr("Open a room. The rest follows."),
                qsTr("Open a room to start a conversation"),
                qsTr("Next conversation, a click away"),
                qsTr("Ready to chat - pick a room"),
                qsTr("All quiet here. Open a room?"),
                qsTr("Chat rooms await - pick one or start your own"),
                qsTr("No room leads to no chat"),
            ];
            return messages[Math.floor(Math.random() * messages.length)];
        }
    }

    TimelineEmptyStateActions {
        Layout.alignment: Qt.AlignHCenter
        dialogHost: root.dialogHost
    }
}
