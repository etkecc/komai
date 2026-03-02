// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "./components"
import "../../components" as Components
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import cc.etke.komai 1.0

Components.OverlayDialog {
    id: roomSettingsDialog

    property var roomSettings

    title: qsTr("Room Settings")
    titleIcon: ":/icons/icons/ui/toggles.svg"

    Item {
        Layout.fillWidth: true
        Layout.preferredHeight: 450
        implicitWidth: parent.width

        Flickable {
            id: flickable

            boundsBehavior: Flickable.StopAtBounds
            anchors.fill: parent
            clip: true
            flickableDirection: Flickable.VerticalFlick
            contentWidth: flickable.width
            contentHeight: contentLayout1.height

            ColumnLayout {
                id: contentLayout1

                width: parent.width
                spacing: Komai.paddingMedium

                RoomSettingsHeaderSection {
                    roomSettings: roomSettingsDialog.roomSettings
                    dialogWidth: flickable.width
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
            }
        }
    }
}
