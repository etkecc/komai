// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import ".."
import "../dialogs"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import QtQml.Models
import im.nheko

Rectangle {
    id: userSettingsDialog

    property int collapsePoint: 600
    property bool collapsed: width < collapsePoint
    property int currentTab: UserSettingsModel.TabLookFeel
    property int sidebarWidth: 200
    color: palette.window

    // Handle Escape key to go back
    focus: true
    Keys.onEscapePressed: mainWindow.pop()

    // Sidebar + Content layout
    RowLayout {
        anchors.fill: parent
        spacing: 0

        // Sidebar
        Rectangle {
            id: sidebar
            Layout.preferredWidth: userSettingsDialog.sidebarWidth
            Layout.fillHeight: true
            color: palette.alternateBase

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                // Header with back button and title - full width clickable
                ItemDelegate {
                    id: headerBack
                    Layout.fillWidth: true
                    Layout.preferredHeight: 48
                    padding: Nheko.paddingSmall
                    leftPadding: Nheko.paddingSmall
                    rightPadding: Nheko.paddingSmall

                    background: Rectangle {
                        color: headerBack.hovered ? palette.dark : "transparent"
                    }

                    onClicked: mainWindow.pop()

                    contentItem: RowLayout {
                        spacing: Nheko.paddingMedium

                        Image {
                            Layout.preferredWidth: 24
                            Layout.preferredHeight: 24
                            Layout.alignment: Qt.AlignVCenter
                            source: "image://colorimage/:/icons/icons/ui/angle-arrow-left.svg?" + (headerBack.hovered ? palette.brightText : palette.text)
                            sourceSize.width: 24
                            sourceSize.height: 24
                        }

                        Label {
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignVCenter
                            text: qsTr("Settings")
                            font.pointSize: fontMetrics.font.pointSize * 1.2
                            font.bold: true
                            color: headerBack.hovered ? palette.brightText : palette.text
                        }
                    }

                    ToolTip.visible: hovered
                    ToolTip.text: qsTr("Back")
                    ToolTip.delay: Nheko.tooltipDelay
                }

                // Navigation items
                ListView {
                    id: navList
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds

                    model: [
                        { text: qsTr("Look & Feel"), icon: "qrc:/icons/icons/ui/toggles.svg", tab: UserSettingsModel.TabLookFeel },
                        { text: qsTr("Timeline"), icon: "qrc:/icons/icons/ui/speech-bubbles.svg", tab: UserSettingsModel.TabTimeline },
                        { text: qsTr("Composer"), icon: "qrc:/icons/icons/ui/edit.svg", tab: UserSettingsModel.TabComposer },
                        { text: qsTr("Notifications"), icon: "qrc:/icons/icons/ui/alert.svg", tab: UserSettingsModel.TabNotifications },
                        { text: qsTr("Calls"), icon: "qrc:/icons/icons/ui/place-call.svg", tab: UserSettingsModel.TabCalls },
                        { text: qsTr("Privacy"), icon: "qrc:/icons/icons/ui/eye-hide.svg", tab: UserSettingsModel.TabPrivacy },
                        { text: qsTr("Encryption"), icon: "qrc:/icons/icons/ui/shield-filled.svg", tab: UserSettingsModel.TabEncryption },
                        { text: qsTr("Session"), icon: "qrc:/icons/icons/ui/person.svg", tab: UserSettingsModel.TabSession },
                        { text: qsTr("About"), icon: "qrc:/logos/komai.svg", tab: UserSettingsModel.TabAbout }
                    ]

                    delegate: ItemDelegate {
                        id: navItem
                        required property var modelData
                        required property int index
                        property bool isActive: userSettingsDialog.currentTab === modelData.tab
                        property color backgroundColor: "transparent"
                        property color textColor: palette.text

                        width: ListView.view.width
                        height: 48
                        padding: Nheko.paddingSmall
                        leftPadding: Nheko.paddingSmall
                        rightPadding: Nheko.paddingSmall

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

                        onClicked: userSettingsDialog.currentTab = modelData.tab

                        contentItem: RowLayout {
                            spacing: Nheko.paddingMedium

                            Image {
                                Layout.preferredWidth: 24
                                Layout.preferredHeight: 24
                                Layout.alignment: Qt.AlignVCenter
                                // Don't colorize the Komai logo (About tab)
                                source: navItem.modelData.icon.startsWith("qrc:/logos/")
                                    ? navItem.modelData.icon
                                    : "image://colorimage/" + navItem.modelData.icon.replace("qrc:/", ":/") + "?" + navItem.textColor
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

        // Separator between sidebar and content
        Rectangle {
            Layout.preferredWidth: 1
            Layout.fillHeight: true
            color: Nheko.theme.separator
        }

        // Settings content - Loader loads only the active tab
        Loader {
            Layout.fillWidth: true
            Layout.fillHeight: true

            source: {
                switch (userSettingsDialog.currentTab) {
                case UserSettingsModel.TabLookFeel:
                    return "settings/LookFeelTab.qml";
                case UserSettingsModel.TabTimeline:
                    return "settings/TimelineTab.qml";
                case UserSettingsModel.TabComposer:
                    return "settings/ComposerTab.qml";
                case UserSettingsModel.TabNotifications:
                    return "settings/NotificationsTab.qml";
                case UserSettingsModel.TabCalls:
                    return "settings/CallsTab.qml";
                case UserSettingsModel.TabPrivacy:
                    return "settings/PrivacyTab.qml";
                case UserSettingsModel.TabEncryption:
                    return "settings/EncryptionTab.qml";
                case UserSettingsModel.TabSession:
                    return "settings/SessionTab.qml";
                case UserSettingsModel.TabAbout:
                    return "settings/AboutTab.qml";
                }
            }

            onLoaded: {
                item.collapsed = Qt.binding(function() { return userSettingsDialog.collapsed; });
            }
        }
    }
}
