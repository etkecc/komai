// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "./components"
import QtQuick 2.15
import QtQuick.Controls 2.3
import QtQuick.Layouts 1.2
import im.nheko 1.0

ApplicationWindow {
    id: roomSettingsDialog

    property var roomSettings

    minimumWidth: 340
    minimumHeight: 450
    width: 450
    height: 680
    color: palette.window
    modality: Qt.NonModal
    flags: Qt.Dialog | Qt.WindowCloseButtonHint | Qt.WindowTitleHint
    title: qsTr("Room Settings")

    Shortcut {
        sequences: [StandardKey.Cancel]
        onActivated: roomSettingsDialog.close()
    }

    Flickable {
        id: flickable

        boundsBehavior: Flickable.StopAtBounds
        anchors.fill: parent
        clip: true
        flickableDirection: Flickable.VerticalFlick
        contentWidth: roomSettingsDialog.width
        contentHeight: contentLayout1.height

        ColumnLayout {
            id: contentLayout1

            width: parent.width
            spacing: Nheko.paddingMedium

            RoomSettingsHeaderSection {
                roomSettings: roomSettingsDialog.roomSettings
                dialogWidth: roomSettingsDialog.width
                Layout.alignment: Qt.AlignHCenter
            }

            RoomSettingsTopicSection {
                id: topicSection

                roomSettings: roomSettingsDialog.roomSettings
                showMoreButtonHeight: showMoreButton.height
                showMoreButtonWidth: showMoreButton.width
                Layout.fillWidth: true
            }

            RoomSettingsDetailsGrid {
                roomSettings: roomSettingsDialog.roomSettings
                timelineRoot: timelineRoot
            }
        }
    }

    Button {
        id: showMoreButton

        anchors.horizontalCenter: flickable.horizontalCenter
        y: Math.min(topicSection.y + topicSection.showMorePlaceholder.y + contentLayout1.y - flickable.contentY,
                    flickable.height - height)
        visible: topicSection.roomTopic.cut
        text: topicSection.roomTopic.showMore ? qsTr("show less") : qsTr("show more")

        onClicked: {
            topicSection.roomTopic.showMore = !topicSection.roomTopic.showMore;
            console.log(flickable.visibleArea);
        }
    }

    footer: DialogButtonBox {
        standardButtons: DialogButtonBox.Ok
        onAccepted: close()
    }
}
