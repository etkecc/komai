// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components" as Components
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai 1.0

Components.OverlayDialog {
    id: roomInfoDialog

    property var roomSettings
    property var members
    property var room
    property var appRoot
    property string initialTab: "settings"
    readonly property string normalizedInitialTab: normalizeTab(initialTab)
    property string currentTab: "settings"
    property bool deferInitialTabSwitch: normalizedInitialTab !== "settings"
    readonly property bool membersTabAvailable: !!members
    overlayViewport: appRoot

    // Close this dialog when the user commits a room upgrade for the same
    // room — once the upgrade is dispatched, "Room Info" of the (about to
    // be tombstoned) old room is stale.  Fires on submit, not on the mere
    // opening of the Upgrade overlay, so a cancelled upgrade keeps Room
    // Info around for the user to come back to.
    Connections {
        target: TimelineManager
        function onRoomUpgradeStarted(roomid) {
            if (roomInfoDialog.roomSettings
                && roomid === roomInfoDialog.roomSettings.roomId) {
                roomInfoDialog.close();
            }
        }
    }
    readonly property int dialogViewportWidth: overlayDialogViewport ? overlayDialogViewport.width : 760
    readonly property int dialogViewportHeight: overlayDialogViewport ? overlayDialogViewport.height : 600
    readonly property int roomInfoDialogWidth: Math.min(
        Math.max(240, dialogViewportWidth - Komai.paddingLarge * 2),
        Math.max(240, Math.floor(dialogViewportWidth * overlayDialogMaxWidthRatio))
    )
    readonly property int roomInfoBodyHeight: Math.max(
        160,
        dialogViewportHeight - overlayDialogChromeHeight - Komai.paddingLarge * 2
    )
    property int sidebarWidth: {
        // Read font height to track font size changes in this binding
        var _d = sidebarNavFontMetrics.height;

        var maxWidth = 0;
        for (var i = 0; i < navModel.length; i++)
            maxWidth = Math.max(maxWidth, sidebarNavFontMetrics.advanceWidth(navModel[i].text));
        return Math.max(180, Math.ceil(Komai.paddingSmall + 24 + Komai.paddingMedium + maxWidth + Komai.paddingSmall));
    }
    property var navModel: {
        const tabs = [
            { text: qsTr("Settings"), icon: ":/icons/icons/ui/settings.svg", tab: "settings" },
            { text: qsTr("Notifications"), icon: ":/icons/icons/ui/alert.svg", tab: "notifications" },
            { text: qsTr("Preferences"), icon: ":/icons/icons/ui/toggles.svg", tab: "preferences" },
            { text: qsTr("Export"), icon: ":/icons/icons/ui/download.svg", tab: "export" },
            { text: qsTr("About this room"), icon: ":/icons/icons/ui/options-circle.svg", tab: "about" }
        ];
        if (membersTabAvailable) {
            tabs.splice(1, 0, {
                "text": qsTr("Members"),
                "icon": ":/icons/icons/ui/people.svg",
                "tab": "members"
            });
        }
        return tabs;
    }

    function normalizeTab(tab) {
        switch (tab) {
        case "settings":
        case "members":
        case "preferences":
        case "notifications":
        case "export":
        case "about":
            return tab;
        default:
            return "settings";
        }
    }

    title: room && room.isSpace ? qsTr("Space Info") : qsTr("Room Info")
    titleIcon: ":/icons/icons/ui/speech-bubbles.svg"
    width: roomInfoDialogWidth
    x: Math.round((dialogViewportWidth - width) / 2)
    y: Math.max(Komai.paddingLarge, Math.round((dialogViewportHeight - height) / 2))

    Component.onCompleted: {
        currentTab = "settings";
    }

    onOpened: {
        if (deferInitialTabSwitch)
            deferredTabSwitchTimer.start();
    }

    onAboutToHide: {
        deferInitialTabSwitch = false;
        deferredTabSwitchTimer.stop();
    }

    Timer {
        id: deferredTabSwitchTimer

        interval: 0
        repeat: false
        running: false
        onTriggered: {
            if (!roomInfoDialog.visible)
                return;

            if (roomInfoDialog.deferInitialTabSwitch)
                roomInfoDialog.currentTab = roomInfoDialog.normalizedInitialTab;
            roomInfoDialog.deferInitialTabSwitch = false;
        }
    }

    FontMetrics {
        id: sidebarNavFontMetrics

        font.bold: true
        font.pointSize: Settings.uiFontSizePt
    }

    Item {
        Layout.fillWidth: true
        Layout.preferredHeight: roomInfoDialog.roomInfoBodyHeight

        RowLayout {
            id: contentRow

            anchors.fill: parent
            spacing: 0

            // Sidebar
            Rectangle {
                id: sidebar

                Layout.preferredWidth: roomInfoDialog.sidebarWidth
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

                        model: roomInfoDialog.navModel

                        delegate: ItemDelegate {
                            id: navItem

                            required property var modelData
                            required property int index
                            property bool isActive: roomInfoDialog.currentTab === modelData.tab
                            property color backgroundColor: palette.window
                            property color textColor: palette.text

                            width: ListView.view.width
                            height: Komai.navigationRowHeight
                            topPadding: (Komai.density !== Settings.Density.Spacious) ? Komai.paddingSmall / 2 : Komai.paddingSmall
                            bottomPadding: (Komai.density !== Settings.Density.Spacious) ? Komai.paddingSmall / 2 : Komai.paddingSmall
                            leftPadding: Komai.paddingSmall
                            rightPadding: Komai.paddingSmall

                            HoverHandler {
                                cursorShape: navItem.isActive ? Qt.ArrowCursor : Qt.PointingHandCursor
                            }

                            background: Rectangle {
                                color: navItem.backgroundColor
                            }

                            readonly property color hoverBackground: Qt.rgba(palette.dark.r * 0.30 + palette.window.r * 0.70, palette.dark.g * 0.30 + palette.window.g * 0.70, palette.dark.b * 0.30 + palette.window.b * 0.70, 1)
                            readonly property color selectedBackground: Qt.rgba(palette.dark.r * 0.85 + palette.window.r * 0.15, palette.dark.g * 0.85 + palette.window.g * 0.15, palette.dark.b * 0.85 + palette.window.b * 0.15, 1)

                            states: [
                                State {
                                    name: "hover"
                                    when: navItem.hovered && !navItem.isActive

                                    PropertyChanges {
                                        navItem {
                                            backgroundColor: navItem.hoverBackground
                                            textColor: palette.text
                                        }
                                    }
                                },
                                State {
                                    name: "active"
                                    when: navItem.isActive

                                    PropertyChanges {
                                        navItem {
                                            backgroundColor: navItem.selectedBackground
                                            textColor: palette.brightText
                                        }
                                    }
                                }
                            ]

                            onClicked: {
                                roomInfoDialog.deferInitialTabSwitch = false;
                                deferredTabSwitchTimer.stop();
                                roomInfoDialog.currentTab = modelData.tab;
                            }

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
                                    font.pointSize: Settings.uiFontSizePt
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
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                color: palette.alternateBase

                Loader {
                    id: tabLoader

                    anchors.fill: parent
                    active: roomInfoDialog.visible
                    asynchronous: true

                    source: {
                        switch (roomInfoDialog.currentTab) {
                        case "settings":
                            return "tabs/RoomInfoSettingsTab.qml";
                        case "members":
                            return "tabs/RoomInfoMembersTab.qml";
                        case "preferences":
                            return "tabs/RoomInfoPreferencesTab.qml";
                        case "notifications":
                            return "tabs/RoomInfoNotificationsTab.qml";
                        case "export":
                            return "tabs/RoomInfoExportTab.qml";
                        case "about":
                            return "tabs/RoomInfoAboutTab.qml";
                        default:
                            return "tabs/RoomInfoSettingsTab.qml";
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
}
