// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai

Page {
    id: roomListPage
    //leftPadding: Komai.paddingSmall
    //rightPadding: Komai.paddingSmall
    required property var timelineRoot
    property bool compactMode: Komai.uiLayoutCompactMode
    property int avatarSize: Komai.listIconSize
    property bool collapsed: false

    ComponentCatalog {
        id: componentCatalog
    }

    background: Rectangle {
        color: Komai.theme.sidebarBackground
    }
    header: ColumnLayout {
        spacing: 0

        RoomListUserInfoPanel {
            id: userInfoPanel
            collapsed: roomListPage.collapsed
            profileContextMenu: profileContextMenu
            Layout.fillWidth: true
        }
        Rectangle {
            Layout.fillWidth: true
            color: Komai.theme.separator
            Layout.preferredHeight: Settings.sidebarsCommunitiesVisible ? 0 : 2
        }
        RoomListActionsBar {
            Layout.fillWidth: true
            Layout.preferredHeight: Komai.navigationRowHeight
            avatarSize: roomListPage.avatarSize
            profileContextMenu: profileContextMenu
            componentCatalog: componentCatalog
            timelineRoot: roomListPage.timelineRoot
        }
        Rectangle {
            Layout.fillWidth: true
            color: Komai.theme.separator
            Layout.preferredHeight: 1
        }
    }

    // HACK: https://bugreports.qt.io/browse/QTBUG-83972, qtwayland cannot auto hide menu
    Connections {
        function onHideMenu() {
            userInfoPanel.closeMenu();
            roomListContextMenu.close();
        }

        target: MainWindow
    }
    RoomListProfileMenu {
        id: profileContextMenu

        timelineRoot: roomListPage.timelineRoot
        componentCatalog: componentCatalog
    }
    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        RoomListSpaceHeader {
            Layout.fillWidth: true
            collapsed: roomListPage.collapsed
            avatarSize: roomListPage.avatarSize
        }

        ListView {
            id: roomlist

            readonly property bool hasVerticalOverflow: contentHeight > height
            readonly property real reservedScrollbarWidth: (!roomListPage.collapsed && Settings.sidebarsRoomListScrollbarsEnabled && hasVerticalOverflow)
                ? Math.max(scrollbar.width, scrollbar.implicitWidth)
                : 0

            Layout.fillWidth: true
            Layout.fillHeight: true
        clip: true
        model: Rooms
        boundsBehavior: Flickable.StopAtBounds

        //reuseItems: true
        ScrollBar.vertical: ScrollBar {
            id: scrollbar

            parent: roomlist
            policy: !collapsed && Settings.sidebarsRoomListScrollbarsEnabled ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
            palette.dark: Qt.darker(parent.palette.alternateBase, 1.5)
            palette.mid: Qt.darker(parent.palette.alternateBase, 1.3)

            Rectangle {
                anchors.fill: parent
                color: palette.window
                z: -1
            }
        }
        delegate: RoomListItemDelegate {
            compactMode: roomListPage.compactMode
            avatarSize: roomListPage.avatarSize
            collapsed: roomListPage.collapsed
            roomContextMenu: roomListContextMenu
            scrollbarReservedWidth: roomlist.reservedScrollbarWidth
        }

        Connections {
            function onCurrentRoomChanged() {
                if (!Rooms.currentRoom)
                    return;

                const roomId = Rooms.currentRoom.roomId;
                Qt.callLater(function () {
                    if (!Rooms.currentRoom || Rooms.currentRoom.roomId !== roomId)
                        return;

                    const index = Rooms.roomidToIndex(roomId);
                    if (index < 0)
                        return;

                    if (TimelineManager.roomSwitchPerfEnabled())
                        TimelineManager.markRoomSwitchPhase(roomId, "qml.room_list.scroll_into_view.begin");
                    roomlist.positionViewAtIndex(index, ListView.Contain);
                    if (TimelineManager.roomSwitchPerfEnabled())
                        TimelineManager.markRoomSwitchPhase(roomId, "qml.room_list.scroll_into_view.end");
                });
            }

            target: Rooms
        }
        Component {
            id: roomWindowComponent

            DetachedRoomWindow {
            }
        }
        RoomListContextMenu {
            id: roomListContextMenu

            timelineRoot: roomListPage.timelineRoot
            roomWindowComponent: roomWindowComponent
        }
        }
    }
}
