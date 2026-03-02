// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "./tabs"
import "../../components" as Components
import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import cc.etke.komai 1.0

Components.OverlayDialog {
    id: roomInfoDialog

    property var roomSettings
    property var members
    property var room
    property var appRoot
    property string initialTab: "settings"
    property string currentTab: initialTab

    title: qsTr("Room Info")
    titleIcon: ":/icons/icons/ui/speech-bubbles.svg"
    width: Math.round((parent ? parent.width : 760) * 0.8)

    Item {
        Layout.fillWidth: true
        Layout.preferredHeight: roomInfoDialog.parent ? roomInfoDialog.parent.height * 0.85 : 600

        RowLayout {
            id: contentRow

            anchors.fill: parent
            spacing: 0

            // Sidebar
            Rectangle {
                id: sidebar

                Layout.preferredWidth: 180
                Layout.fillHeight: true
                color: palette.alternateBase

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    ListView {
                        id: navList

                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        boundsBehavior: Flickable.StopAtBounds
                        interactive: false

                        model: [
                            { text: qsTr("Settings"), icon: ":/icons/icons/ui/toggles.svg", tab: "settings" },
                            { text: qsTr("Members"), icon: ":/icons/icons/ui/people.svg", tab: "members" },
                            { text: qsTr("Notifications"), icon: ":/icons/icons/ui/alert.svg", tab: "notifications" },
                            { text: qsTr("About"), icon: ":/icons/icons/ui/options-circle.svg", tab: "about" }
                        ]

                        delegate: ItemDelegate {
                            id: navItem

                            required property var modelData
                            required property int index
                            property bool isActive: roomInfoDialog.currentTab === modelData.tab
                            property color backgroundColor: palette.window
                            property color textColor: palette.text

                            width: ListView.view.width
                            height: Komai.navigationRowHeight
                            padding: Komai.paddingSmall
                            leftPadding: Komai.paddingSmall
                            rightPadding: Komai.paddingSmall

                            HoverHandler {
                                cursorShape: navItem.isActive ? Qt.ArrowCursor : Qt.PointingHandCursor
                            }

                            background: Rectangle {
                                color: navItem.backgroundColor
                            }

                            states: [
                                State {
                                    name: "hover"
                                    when: navItem.hovered && !navItem.isActive

                                    PropertyChanges {
                                        navItem {
                                            backgroundColor: palette.dark
                                            textColor: palette.brightText
                                        }
                                    }
                                },
                                State {
                                    name: "active"
                                    when: navItem.isActive

                                    PropertyChanges {
                                        navItem {
                                            backgroundColor: palette.highlight
                                            textColor: palette.highlightedText
                                        }
                                    }
                                }
                            ]

                            onClicked: roomInfoDialog.currentTab = modelData.tab

                            contentItem: RowLayout {
                                spacing: Komai.paddingMedium

                                Image {
                                    Layout.preferredWidth: 24
                                    Layout.preferredHeight: 24
                                    Layout.alignment: Qt.AlignVCenter
                                    source: "image://colorimage/" + navItem.modelData.icon + "?" + navItem.textColor
                                    sourceSize.width: 24
                                    sourceSize.height: 24
                                }

                                Label {
                                    Layout.fillWidth: true
                                    Layout.alignment: Qt.AlignVCenter
                                    text: navItem.modelData.text
                                    color: navItem.textColor
                                    font.bold: navItem.isActive
                                    elide: Text.ElideRight
                                }
                            }
                        }
                    }
                }
            }

            // Separator
            Rectangle {
                Layout.preferredWidth: 1
                Layout.fillHeight: true
                color: Komai.theme.separator
            }

            // Content area
            Loader {
                id: tabLoader

                Layout.fillWidth: true
                Layout.fillHeight: true

                source: {
                    switch (roomInfoDialog.currentTab) {
                    case "settings":
                        return "tabs/RoomInfoSettingsTab.qml";
                    case "members":
                        return "tabs/RoomInfoMembersTab.qml";
                    case "notifications":
                        return "tabs/RoomInfoNotificationsTab.qml";
                    case "about":
                        return "tabs/RoomInfoAboutTab.qml";
                    }
                }

                onLoaded: {
                    if (item) {
                        item.roomSettings = roomInfoDialog.roomSettings;
                        item.members = roomInfoDialog.members;
                        item.room = roomInfoDialog.room;
                        item.appRoot = roomInfoDialog.appRoot;
                    }
                }
            }
        }
    }
}
