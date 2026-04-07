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
    color: palette.window

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
                preferredWidth: Settings.sidebarsCommunitiesWidthPx > collapsedWidth
                                ? Settings.sidebarsCommunitiesWidthPx
                                : collapsedWidth
                visible: Settings.sidebarsCommunitiesVisible

                CommunitiesList {
                    id: communitiesList

                    adaptiveView: adaptiveView
                    collapsed: parent.collapsed
                    roomListTarget: roomlist
                }
                Binding {
                    delayed: true
                    property: 'sidebarsCommunitiesWidthPx'
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
                preferredWidth: Math.max(Settings.sidebarsRoomListWidthPx, collapsedWidth)

                RoomList {
                    id: roomlist

                    adaptiveView: adaptiveView
                    collapsed: parent.collapsed
                    communitiesTarget: communitiesList
                    height: adaptiveView.height
                    timelineRoot: chatPage.timelineRoot
                }
                Binding {
                    delayed: true
                    property: 'sidebarsRoomListWidthPx'
                    restoreMode: Binding.RestoreBindingOrValue
                    target: Settings
                    value: roomListC.preferredWidth
                    when: !adaptiveView.singlePageMode
                }
            }
            AdaptiveLayoutElement {
                id: timlineViewC

                minimumWidth: fontMetrics.averageCharacterWidth * 40 + Komai.listIconSize + 2 * Komai.paddingMedium

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

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
