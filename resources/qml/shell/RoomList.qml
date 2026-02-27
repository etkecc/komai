// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "./components"
import "../components"
import "../dialogs/common"
import "../dialogs/room"
import "../ui"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import im.nheko

Page {
    id: roomListPage
    //leftPadding: Nheko.paddingSmall
    //rightPadding: Nheko.paddingSmall
    property bool compactMode: Nheko.uiLayoutCompactMode
    property int avatarSize: Nheko.listIconSize
    property bool collapsed: false

    ComponentCatalog {
        id: componentCatalog
    }

    background: Rectangle {
        color: Nheko.theme.sidebarBackground
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
            color: Nheko.theme.separator
            Layout.preferredHeight: Settings.sidebarsCommunitiesVisible ? 0 : 2
        }
        RoomListActionsBar {
            Layout.fillWidth: true
            Layout.preferredHeight: Nheko.navigationRowHeight
            avatarSize: roomListPage.avatarSize
            profileContextMenu: profileContextMenu
            componentCatalog: componentCatalog
            timelineRoot: timelineRoot
        }
        Rectangle {
            Layout.fillWidth: true
            color: Nheko.theme.separator
            Layout.preferredHeight: 1
        }
    }

    // HACK: https://bugreports.qt.io/browse/QTBUG-83972, qtwayland cannot auto hide menu
    Connections {
        function onHideMenu() {
            userInfoPanel.closeMenu();
            roomContextMenu.close();
        }

        target: MainWindow
    }
    RoomListProfileMenu {
        id: profileContextMenu

        timelineRoot: timelineRoot
        componentCatalog: componentCatalog
        createRoomComponent: createRoomComponent
        createDirectComponent: createDirectComponent
        roomDirectoryComponent: roomDirectoryComponent
    }

    Component {
        id: roomDirectoryComponent

        RoomDirectory {
        }
    }
    Component {
        id: createRoomComponent

        CreateRoom {
        }
    }
    Component {
        id: createDirectComponent

        CreateDirect {
        }
    }
    ListView {
        id: roomlist

        anchors.left: parent.left
        anchors.right: parent.right
        height: parent.height
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
            roomContextMenu: roomContextMenu
            scrollbar: scrollbar
        }

        Connections {
            function onCurrentRoomChanged() {
                if (Rooms.currentRoom)
                    roomlist.positionViewAtIndex(Rooms.roomidToIndex(Rooms.currentRoom.roomId), ListView.Contain);
            }

            target: Rooms
        }
        Component {
            id: roomWindowComponent

            DetachedRoomWindow {
            }
        }
        RoomListContextMenu {
            id: roomContextMenu

            timelineRoot: timelineRoot
            roomWindowComponent: roomWindowComponent
        }
    }
}
