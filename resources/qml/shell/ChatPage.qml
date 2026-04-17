// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Layouts
import cc.etke.komai 1.0

// this needs to be last
import QtQml

Rectangle {
    id: chatPage

    required property var timelineRoot
    readonly property Item roomListLastActionButton: roomlist.roomListLastActionButton
    readonly property var notificationAreaItem: timeline.notificationAreaItem
    readonly property var notificationAvoidBottomItem: timeline.notificationAvoidBottomItem
    readonly property alias tabController: tabController
    color: palette.window

    RoomTabController {
        id: tabController

        Component.onCompleted: restoreTabs()
    }

    // Handle external room changes (quick switcher, etc.) for tab-aware navigation.
    Connections {
        target: Rooms

        function onCurrentRoomIdChanged(currentRoomId) {
            tabController.handleExternalRoomChange(currentRoomId);
        }
    }

    // Tab keyboard shortcuts.
    Shortcut { sequence: "Alt+1"; onActivated: tabController.switchToTab(0) }
    Shortcut { sequence: "Alt+2"; onActivated: tabController.switchToTab(1) }
    Shortcut { sequence: "Alt+3"; onActivated: tabController.switchToTab(2) }
    Shortcut { sequence: "Alt+4"; onActivated: tabController.switchToTab(3) }
    Shortcut { sequence: "Alt+5"; onActivated: tabController.switchToTab(4) }
    Shortcut { sequence: "Alt+6"; onActivated: tabController.switchToTab(5) }
    Shortcut { sequence: "Alt+7"; onActivated: tabController.switchToTab(6) }
    Shortcut { sequence: "Alt+8"; onActivated: tabController.switchToTab(7) }
    Shortcut { sequence: "Alt+9"; onActivated: tabController.switchToTab(8) }
    Shortcut {
        sequence: "Ctrl+T"

        onActivated: tabController.openNewTab()
    }
    Shortcut {
        sequences: ["Ctrl+K", "Ctrl+P"]

        onActivated: tabController.openNewTab()
    }
    Shortcut {
        sequences: ["Ctrl+N", "Ctrl+Shift+N"]
        context: Qt.ApplicationShortcut

        onActivated: roomJoinCreateDialog.open()
        onActivatedAmbiguously: roomJoinCreateDialog.open()
    }
    RoomJoinCreateDialog {
        id: roomJoinCreateDialog
        dialogHost: timeline
    }
    Shortcut {
        sequence: "Ctrl+W"
        enabled: tabController.tabs.count > 0

        onActivated: tabController.closeCurrentTab()
    }
    Shortcut {
        sequence: "Ctrl+Shift+T"
        enabled: tabController._closedTabsStack.length > 0

        onActivated: tabController.reopenClosedTab()
    }
    Shortcut {
        sequence: "Ctrl+Tab"
        enabled: tabController.tabs.count > 1

        onActivated: tabController.nextTab()
    }
    Shortcut {
        sequence: "Ctrl+Shift+Tab"
        enabled: tabController.tabs.count > 1

        onActivated: tabController.previousTab()
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        AdaptiveLayout {
            id: adaptiveView

            function initializePageIndex() {
                if (!singlePageMode)
                    adaptiveView.pageIndex = 0;
                else if (Rooms.currentRoomPreview.roomid)
                    adaptiveView.pageIndex = 2;
                else
                    adaptiveView.pageIndex = 1;
            }

            Layout.fillHeight: true
            Layout.fillWidth: true
            pageIndex: 1
            singlePageMode: communityListC.preferredWidth + roomListC.preferredWidth + timlineViewC.minimumWidth > width

            Component.onCompleted: initializePageIndex()
            onSinglePageModeChanged: initializePageIndex()

            Connections {
                function onCurrentRoomIdChanged() {
                    adaptiveView.initializePageIndex();
                }

                target: Rooms
            }
            AdaptiveLayoutElement {
                id: communityListC

                collapsedWidth: Math.max(Komai.navigationRowHeight, 1)
                maximumWidth: Math.min(500, adaptiveView.width * 0.5)
                preferredWidth: Settings.navigationCommunitiesWidthPx > collapsedWidth
                                ? Settings.navigationCommunitiesWidthPx
                                : collapsedWidth
                visible: true

                CommunitiesList {
                    id: communitiesList

                    adaptiveView: adaptiveView
                    collapsed: parent.collapsed
                    roomListTarget: roomlist
                }
                Binding {
                    delayed: true
                    property: 'navigationCommunitiesWidthPx'
                    restoreMode: Binding.RestoreBindingOrValue
                    target: Settings
                    value: communityListC.preferredWidth
                    when: !adaptiveView.singlePageMode
                }
            }
            AdaptiveLayoutElement {
                id: roomListC

                maximumWidth: Math.min(500, adaptiveView.width * 0.5)
                collapsedWidth: Math.max(Komai.navigationRowHeight, 1)
                preferredWidth: Math.max(Settings.navigationRoomListWidthPx, collapsedWidth)

                RoomList {
                    id: roomlist

                    adaptiveView: adaptiveView
                    collapsed: parent.collapsed
                    communitiesTarget: communitiesList
                    height: adaptiveView.height
                    timelineRoot: chatPage.timelineRoot
                    tabController: chatPage.tabController
                }
                Binding {
                    delayed: true
                    property: 'navigationRoomListWidthPx'
                    restoreMode: Binding.RestoreBindingOrValue
                    target: Settings
                    value: roomListC.preferredWidth
                    when: !adaptiveView.singlePageMode
                }
            }
            AdaptiveLayoutElement {
                id: timlineViewC

                minimumWidth: fontMetrics.averageCharacterWidth * 40 + Komai.iconSize + 2 * Komai.paddingMedium

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    RoomTabBar {
                        Layout.fillWidth: true
                        tabController: chatPage.tabController
                        visible: !adaptiveView.singlePageMode && tabController.tabs.count > 0
                    }
                    NetworkConnectivityBanner {
                        Layout.fillWidth: true
                    }
                    SelfVerificationBanner {
                        Layout.fillWidth: true
                    }
                    TimelineView {
                        id: timeline

                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        tabController: chatPage.tabController
                        dialogHost: chatPage.timelineRoot
                        roomListLastActionButton: chatPage.roomListLastActionButton
                        windowFocusBlurOverlay: windowFocusBlurOverlay
                        roomPreview: Rooms.currentRoomPreview.roomid ? Rooms.currentRoomPreview : null
                        showBackButton: adaptiveView.singlePageMode
                    }
                }
            }
        }
    }
    PrivacyScreen {
        id: windowFocusBlurOverlay

        anchors.fill: parent
        screenTimeout: Settings.desktopWindowFocusBlurDelaySeconds
        timelineRoot: adaptiveView
        visible: Settings.desktopWindowFocusBlurEnabled
        windowTarget: MainWindow
    }
}
