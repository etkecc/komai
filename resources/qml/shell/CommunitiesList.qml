// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "./components"
import QtQml
import QtQuick
import QtQuick.Controls
import im.nheko

Page {
    id: communitySidebar

    //leftPadding: Nheko.paddingSmall
    //rightPadding: Nheko.paddingSmall
    property int avatarSize: Nheko.listIconSize
    property bool collapsed: false

    background: Rectangle {
        color: Nheko.theme.sidebarBackground
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
            fontMetrics: fontMetrics
            scrollbar: scrollbar
        }

        CommunitiesContextMenu {
            id: communityContextMenu
        }
    }
}
