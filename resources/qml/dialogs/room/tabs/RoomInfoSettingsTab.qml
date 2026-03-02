// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../components"
import "../../../components" as Components
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
            spacing: Komai.paddingMedium

            RoomSettingsHeaderSection {
                roomSettings: settingsTab.roomSettings
                dialogWidth: scrollView.availableWidth
                Layout.alignment: Qt.AlignHCenter
            }

            RoomSettingsTopicSection {
                id: topicSection

                roomSettings: settingsTab.roomSettings
                showMoreButtonHeight: showMoreButton.height
                showMoreButtonWidth: showMoreButton.width
                Layout.fillWidth: true
            }

            RoomSettingsDetailsGrid {
                roomSettings: settingsTab.roomSettings
                appRoot: settingsTab.appRoot
            }
        }
    }

    Button {
        id: showMoreButton

        anchors.horizontalCenter: scrollView.horizontalCenter
        y: Math.min(topicSection.y + topicSection.showMorePlaceholder.y - scrollView.contentItem.contentY,
                    scrollView.height - height)
        visible: topicSection.roomTopic.cut
        text: topicSection.roomTopic.showMore ? qsTr("show less") : qsTr("show more")

        onClicked: {
            topicSection.roomTopic.showMore = !topicSection.roomTopic.showMore;
        }
    }
}
