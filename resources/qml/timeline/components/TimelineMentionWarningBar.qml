// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai

Rectangle {
    id: mentionPopup

    required property string mention
    required property int mentionIndex
    required property bool replyPopupVisible
    required property var room

    property bool isRoomMention: mention === "@room"
    property bool isTopMostBar: !replyPopupVisible && mentionIndex === 0
    property int headerTextHeight: Math.round(Komai.fontPixelSize * 2.4)
    property int headerIconSize: Math.ceil(mentionPopup.headerTextHeight * 0.5)
    property int headerFontSize: Math.ceil(mentionPopup.headerTextHeight * 0.45)

    Layout.fillWidth: true
    color: palette.alternateBase
    radius: mentionPopup.isTopMostBar ? 8 : 0
    implicitHeight: mentionRow.implicitHeight + Komai.paddingMedium * 2
    z: 3

    // Keep only top corners rounded so the bar sits flush above the composer.
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: parent.radius
        color: parent.color
        visible: mentionPopup.isTopMostBar
    }

    RowLayout {
        id: mentionRow

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.margins: Komai.paddingMedium
        spacing: Komai.paddingSmall

        Image {
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredHeight: mentionPopup.headerIconSize
            Layout.preferredWidth: mentionPopup.headerIconSize
            source: mentionPopup.isRoomMention ? "image://colorimage/:/icons/icons/ui/mention.svg?" + Komai.theme.error : "image://colorimage/:/icons/icons/ui/person.svg?" + palette.text
        }

        Label {
            Layout.fillWidth: true
            color: palette.text
            elide: Text.ElideRight
            font.bold: true
            font.pixelSize: mentionPopup.headerFontSize
            text: mentionPopup.isRoomMention ? qsTr("You are about to notify the whole room") : qsTr("You are about to mention %1").arg(mentionPopup.mention)
        }

        ImageButton {
            toolTipText: qsTr("Don't mention them in this message")
            toolTipVisible: hovered
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredHeight: mentionPopup.headerIconSize
            Layout.preferredWidth: mentionPopup.headerIconSize
            hoverEnabled: true
            image: ":/icons/icons/ui/dismiss.svg"

            onClicked: room.input.removeMention(mentionPopup.mention)
        }
    }
}
