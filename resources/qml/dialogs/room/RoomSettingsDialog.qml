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
    property var appRoot

    title: qsTr("Room Settings")
    titleIcon: ":/icons/icons/ui/toggles.svg"
    width: Math.round((parent ? parent.width : 760) * 0.8)

    Item {
        Layout.fillWidth: true
        Layout.preferredHeight: Math.min(scrollContent.implicitHeight + Komai.paddingMedium,
                                         roomSettingsDialog.parent ? roomSettingsDialog.parent.height * 0.85 : 600)

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
                    roomSettings: roomSettingsDialog.roomSettings
                    dialogWidth: scrollView.availableWidth
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
                    appRoot: roomSettingsDialog.appRoot
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
}
