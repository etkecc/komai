// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.3
import "components"
import "components/banners" as Banners
import im.nheko 1.0

// this needs to be last
import QtQml 2.15

Rectangle {
    id: chatPage

    color: palette.window

    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        AdaptiveLayout {
            id: adaptiveView

            function initializePageIndex() {
                if (!singlePageMode)
                    adaptiveView.pageIndex = 0;
                else if (Rooms.currentRoom || Rooms.currentRoomPreview.roomid)
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
                function onCurrentRoomChanged() {
                    adaptiveView.initializePageIndex();
                }

                target: Rooms
            }
            AdaptiveLayoutElement {
                id: communityListC

                collapsedWidth: Math.max(communitiesList.avatarSize + 2 * Nheko.paddingMedium, 1)
                maximumWidth: Math.min(500, adaptiveView.width * 0.5)
                preferredWidth: Settings.communityListWidth > collapsedWidth
                                ? Settings.communityListWidth
                                : collapsedWidth
                visible: Settings.sidebarsCommunitiesVisible

                CommunitiesList {
                    id: communitiesList

                    collapsed: parent.collapsed
                }
                Binding {
                    delayed: true
                    property: 'communityListWidth'
                    restoreMode: Binding.RestoreBindingOrValue
                    target: Settings
                    value: communityListC.preferredWidth
                    when: !adaptiveView.singlePageMode
                }
            }
            AdaptiveLayoutElement {
                id: roomListC

                maximumWidth: Math.min(500, adaptiveView.width * 0.5)
                collapsedWidth: Math.max(roomlist.avatarSize + 2 * Nheko.paddingMedium, 1)
                preferredWidth: Math.max(Settings.roomListWidth, collapsedWidth)

                RoomList {
                    id: roomlist

                    collapsed: parent.collapsed
                    height: adaptiveView.height
                }
                Binding {
                    delayed: true
                    property: 'roomListWidth'
                    restoreMode: Binding.RestoreBindingOrValue
                    target: Settings
                    value: roomListC.preferredWidth
                    when: !adaptiveView.singlePageMode
                }
            }
            AdaptiveLayoutElement {
                id: timlineViewC

                minimumWidth: fontMetrics.averageCharacterWidth * 40 + Nheko.avatarSize + 2 * Nheko.paddingMedium

                ColumnLayout {
                    anchors.fill: parent
                    spacing: 0

                    Banners.NetworkConnectivityBanner {
                        Layout.fillWidth: true
                    }
                    Banners.SelfVerificationBanner {
                        Layout.fillWidth: true
                    }
                    TimelineView {
                        id: timeline

                        Layout.fillHeight: true
                        Layout.fillWidth: true
                        windowFocusBlurOverlay: windowFocusBlurOverlay
                        room: Rooms.currentRoom
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
        screenTimeout: Settings.windowFocusBlurDelaySeconds
        timelineRoot: adaptiveView
        visible: Settings.windowFocusBlurEnabled
        windowTarget: MainWindow
    }
}
