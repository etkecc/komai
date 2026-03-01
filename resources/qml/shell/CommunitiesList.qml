// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQml
import QtQuick
import QtQuick.Controls
import cc.etke.komai

Page {
    id: communitySidebar

    //leftPadding: Komai.paddingSmall
    //rightPadding: Komai.paddingSmall
    property int avatarSize: Komai.listIconSize
    property bool collapsed: false

    background: Rectangle {
        color: Komai.theme.sidebarBackground
    }

    // HACK: https://bugreports.qt.io/browse/QTBUG-83972, qtwayland cannot auto hide menu
    Connections {
        function onHideMenu() {
            communityContextMenu.close();
        }

        target: MainWindow
    }
    ListView {
        id: communitiesList

        readonly property bool hasVerticalOverflow: contentHeight > height
        readonly property real reservedScrollbarWidth: (!communitySidebar.collapsed && Settings.sidebarsRoomListScrollbarsEnabled && hasVerticalOverflow)
            ? Math.max(scrollbar.width, scrollbar.implicitWidth)
            : 0

        anchors.left: parent.left
        anchors.right: parent.right
        height: parent.height
        model: Communities.filtered()
        boundsBehavior: Flickable.StopAtBounds

        ScrollBar.vertical: ScrollBar {
            id: scrollbar

            parent: communitiesList
            policy: !collapsed && Settings.sidebarsRoomListScrollbarsEnabled ? ScrollBar.AlwaysOn : ScrollBar.AlwaysOff
            palette.dark: Qt.darker(parent.palette.alternateBase, 1.5)
            palette.mid: Qt.darker(parent.palette.alternateBase, 1.3)

            Rectangle {
                anchors.fill: parent
                color: palette.window
                z: -1
            }
        }
        delegate: CommunitiesListItemDelegate {
            avatarSize: communitySidebar.avatarSize
            collapsed: communitySidebar.collapsed
            communityContextMenu: communityContextMenu
            scrollbarReservedWidth: communitiesList.reservedScrollbarWidth
        }

        CommunitiesContextMenu {
            id: communityContextMenu
        }
    }
}
