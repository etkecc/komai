// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../../components"
import QtQuick 2.15
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.2
import cc.etke.komai 1.0

ColumnLayout {
    required property var roomSettings
    required property int showMoreButtonHeight
    required property int showMoreButtonWidth
    property alias roomTopic: roomTopic
    property alias showMorePlaceholder: showMorePlaceholder

    TextArea {
        id: roomTopic

        property bool cut: implicitHeight > 100
        property bool showMore: false
        property bool isTopicEditingAllowed: false

        clip: true
        Layout.maximumHeight: showMore ? Number.POSITIVE_INFINITY : 100
        Layout.preferredHeight: implicitHeight
        Layout.alignment: Qt.AlignHCenter
        Layout.fillWidth: true
        Layout.leftMargin: Komai.paddingLarge
        Layout.rightMargin: Komai.paddingLarge

        readOnly: !isTopicEditingAllowed
        textFormat: isTopicEditingAllowed ? TextEdit.PlainText : TextEdit.RichText
        text: isTopicEditingAllowed
            ? roomSettings.plainRoomTopic
            : (roomSettings.plainRoomTopic === "" ? ("<i>" + qsTr("No topic set") + "</i>") : roomSettings.roomTopic)
        wrapMode: TextEdit.WordWrap
        background: null
        color: palette.text
        horizontalAlignment: TextEdit.AlignHCenter

        onLinkActivated: Komai.openLink(link)

        KomaiCursorShape {
            anchors.fill: parent
            cursorShape: parent.hoveredLink ? Qt.PointingHandCursor : Qt.ArrowCursor
        }
    }

    RowLayout {
        spacing: Komai.paddingMedium
        Layout.alignment: Qt.AlignHCenter

        ImageButton {
            id: topicChangeButton

            visible: roomSettings.canChangeTopic
            hoverEnabled: true
            ToolTip.visible: hovered
            ToolTip.text: qsTr("Change topic of this room")
            ToolTip.delay: Komai.tooltipDelay
            image: roomTopic.isTopicEditingAllowed ? ":/icons/icons/ui/checkmark.svg" : ":/icons/icons/ui/edit.svg"

            onClicked: {
                if (roomTopic.isTopicEditingAllowed) {
                    roomSettings.changeTopic(roomTopic.text);
                    roomTopic.isTopicEditingAllowed = false;
                } else {
                    roomTopic.isTopicEditingAllowed = true;
                    roomTopic.showMore = true;
                    roomTopic.focus = true;
                    // roomTopic.selectAll();
                }
            }
        }

        EncryptionIndicator {
            Layout.preferredHeight: 16
            Layout.preferredWidth: 16
            sourceSize.width: width
            sourceSize.height: height
            encrypted: true
            visible: roomSettings.isEncryptionEnabled && (roomSettings.plainRoomTopic !== "" || !roomTopic.readOnly)
            trust: Crypto.Unverified
            ToolTip.text: qsTr("Since room state can't be encrypted, make sure no confidential information is stored in the room topic!")
        }
    }

    Item {
        id: showMorePlaceholder

        Layout.alignment: Qt.AlignHCenter
        Layout.preferredHeight: showMoreButtonHeight
        Layout.preferredWidth: showMoreButtonWidth
        visible: roomTopic.cut
    }
}
