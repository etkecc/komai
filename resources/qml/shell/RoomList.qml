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

        Pane {
            id: userInfoPanel
            Layout.maximumHeight: Settings.sidebarsCommunitiesVisible ? 0 : -1
            clip: true

            function openUserProfile() {
                profileContextMenu.openCurrentUserProfile();
            }

            Layout.alignment: Qt.AlignBottom
            Layout.fillWidth: true
            Layout.minimumHeight: 40
            //Layout.preferredHeight: userInfoGrid.implicitHeight + 2 * Nheko.paddingMedium
            padding: Nheko.paddingMedium

            background: Rectangle {
                color: palette.window
            }
            contentItem: RowLayout {
                id: userInfoGrid

                property var profile: Nheko.currentUser

                spacing: Nheko.paddingMedium

                Avatar {
                    id: headerAvatar

                    Layout.alignment: Qt.AlignVCenter
                    Layout.preferredHeight: fontMetrics.lineSpacing * 2
                    Layout.preferredWidth: fontMetrics.lineSpacing * 2
                    displayName: userInfoGrid.profile ? userInfoGrid.profile.displayName : ""
                    enabled: false
                    url: (userInfoGrid.profile ? userInfoGrid.profile.avatarUrl : "").replace("mxc://", "image://MxcImage/")
                    userid: userInfoGrid.profile ? userInfoGrid.profile.userid : ""
                }
                ColumnLayout {
                    id: col

                    Layout.alignment: Qt.AlignLeft
                    Layout.fillWidth: true
                    Layout.preferredWidth: parent.width - headerAvatar.width - logoutButton.width - Nheko.paddingMedium * 2
                    spacing: 0
                    visible: !collapsed

                    ElidedLabel {
                        Layout.alignment: Qt.AlignBottom
                        elideWidth: col.width
                        font.pointSize: Settings.uiFontSizePt * 1.1
                        font.weight: Font.DemiBold
                        fullText: userInfoGrid.profile ? userInfoGrid.profile.displayName : ""
                    }
                    ElidedLabel {
                        Layout.alignment: Qt.AlignTop
                        color: palette.buttonText
                        elideWidth: col.width
                        font.pointSize: Settings.uiFontSizePt * 0.9
                        fullText: userInfoGrid.profile ? userInfoGrid.profile.userid : ""
                    }
                }
                Item {
                }
                ImageButton {
                    id: logoutButton

                    Layout.alignment: Qt.AlignVCenter
                    Layout.preferredHeight: fontMetrics.lineSpacing * 2
                    Layout.preferredWidth: fontMetrics.lineSpacing * 2
                    ToolTip.delay: Nheko.tooltipDelay
                    ToolTip.text: qsTr("Logout")
                    ToolTip.visible: hovered
                    image: ":/icons/icons/ui/power-off.svg"
                    visible: !collapsed

                    onClicked: Nheko.openLogoutDialog()
                }
            }

            InputDialog {
                id: statusDialog

                prompt: qsTr("Enter your status message:")
                title: qsTr("Status Message")

                text: userInfoGrid.profile ? Presence.userStatus(userInfoGrid.profile.userid) : ""

                onAccepted: function (text) {
                    Nheko.setStatusMessage(text);
                }
            }
            Menu {
                id: userInfoMenu

                MenuItem {
                    text: qsTr("Profile settings")

                    onTriggered: userInfoPanel.openUserProfile()
                }
                MenuItem {
                    text: qsTr("Set status message")

                    onTriggered: statusDialog.show()
                }
                MenuSeparator {
                }

                ButtonGroup {
                    id: onlineStateGroup
                }
                MenuItem {
                    text: qsTr("Automatic online status")
                    ButtonGroup.group: onlineStateGroup
                    checkable: true
                    checked: Settings.networkPresenceStatusPolicy == Settings.Presence.AutomaticPresence
                    onTriggered: if (checked) Settings.networkPresenceStatusPolicy = Settings.Presence.AutomaticPresence
                }
                MenuItem {
                    text: qsTr("Online")
                    ButtonGroup.group: onlineStateGroup
                    checkable: true
                    checked: Settings.networkPresenceStatusPolicy == Settings.Presence.Online
                    onTriggered: if (checked) Settings.networkPresenceStatusPolicy = Settings.Presence.Online
                }
                MenuItem {
                    text: qsTr("Unavailable")
                    ButtonGroup.group: onlineStateGroup
                    checkable: true
                    checked: Settings.networkPresenceStatusPolicy == Settings.Presence.Unavailable
                    onTriggered: if (checked) Settings.networkPresenceStatusPolicy = Settings.Presence.Unavailable
                }
                MenuItem {
                    text: qsTr("Offline")
                    ButtonGroup.group: onlineStateGroup
                    checkable: true
                    checked: Settings.networkPresenceStatusPolicy == Settings.Presence.Offline
                    onTriggered: if (checked) Settings.networkPresenceStatusPolicy = Settings.Presence.Offline
                }
            }
            TapHandler {
                acceptedButtons: Qt.LeftButton
                gesturePolicy: TapHandler.ReleaseWithinBounds
                margin: -Nheko.paddingSmall

                onLongPressed: userInfoMenu.popup(userInfoPanel)
                onSingleTapped: userInfoPanel.openUserProfile()
            }
            TapHandler {
                acceptedButtons: Qt.RightButton
                gesturePolicy: TapHandler.ReleaseWithinBounds
                margin: -Nheko.paddingSmall

                onSingleTapped: userInfoMenu.popup(userInfoPanel)
            }
        }
        Rectangle {
            Layout.fillWidth: true
            color: Nheko.theme.separator
            Layout.preferredHeight: Settings.sidebarsCommunitiesVisible ? 0 : 2
        }
        Pane {
            id: roomActionsBar

            property int buttonSize: Math.min(30, avatarSize)
            property bool showActionButtons: roomActionsBar.width > 160

            Layout.fillWidth: true
            Layout.preferredHeight: Nheko.navigationRowHeight
            horizontalPadding: Nheko.paddingMedium
            verticalPadding: 0

            background: Rectangle {
                color: palette.alternateBase
            }
            contentItem: RowLayout {
                id: buttonRow

                spacing: Nheko.paddingMedium

                UserSettingsFlipButton {
                    id: userSettingsButton

                    profile: Nheko.currentUser
                    avatarButtonSize: Nheko.barIconSize

                    Layout.preferredHeight: Nheko.navigationRowHeight
                    Layout.preferredWidth: effectiveButtonSize
                    onLeftClicked: {
                        if (!roomActionsBar.showActionButtons)
                            profileContextMenu.popup(userSettingsButton)
                        else
                            MainWindow.showUserSettingsPage()
                    }

                    onRightClicked: profileContextMenu.popup(userSettingsButton)
                }
                Item {
                    Layout.fillWidth: true
                    visible: roomActionsBar.showActionButtons
                }
                ImageButton {
                    id: startChatButton

                    ToolTip.delay: Nheko.tooltipDelay
                    ToolTip.text: qsTr("Start a new chat")
                    ToolTip.visible: hovered
                    Layout.preferredHeight: roomActionsBar.buttonSize
                    Layout.preferredWidth: roomActionsBar.buttonSize
                    hoverEnabled: true
                    image: ":/icons/icons/ui/plus-circle.svg"
                    visible: roomActionsBar.showActionButtons

                    onClicked: roomJoinCreateMenu.popup(startChatButton)

                    Menu {
                        id: roomJoinCreateMenu

                        MenuItem {
                            text: qsTr("Join a room")

                            onTriggered: Nheko.openJoinRoomDialog()
                        }
                        MenuItem {
                            text: qsTr("Create a new room")

                            onTriggered: profileContextMenu.openCreateRoomDialog({})
                        }
                        MenuItem {
                            text: qsTr("Start a direct chat")

                            onTriggered: profileContextMenu.openCreateDirectDialog()
                        }
                        MenuItem {
                            text: qsTr("Create a new community")

                            onTriggered: profileContextMenu.openCreateRoomDialog({
                                    "space": true
                                })
                        }
                    }
                }
                ImageButton {
                    ToolTip.delay: Nheko.tooltipDelay
                    ToolTip.text: qsTr("Room directory")
                    ToolTip.visible: hovered
                    Layout.preferredHeight: roomActionsBar.buttonSize
                    Layout.preferredWidth: roomActionsBar.buttonSize
                    hoverEnabled: true
                    image: ":/icons/icons/ui/room-directory.svg"
                    visible: roomActionsBar.showActionButtons

                    onClicked: profileContextMenu.openRoomDirectoryDialog()
                }
                ImageButton {
                    ToolTip.delay: Nheko.tooltipDelay
                    ToolTip.text: qsTr("Find & switch room (Ctrl+K)")
                    ToolTip.visible: hovered
                    Layout.preferredHeight: roomActionsBar.buttonSize
                    Layout.preferredWidth: roomActionsBar.buttonSize
                    hoverEnabled: true
                    image: ":/icons/icons/ui/search.svg"
                    ripple: false
                    visible: roomActionsBar.showActionButtons

                    onClicked: timelineRoot.openCatalogDialog(componentCatalog.navigationQuickSwitcherDialog)
                }
            }
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
            userInfoMenu.close();
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
            fontMetrics: fontMetrics
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
