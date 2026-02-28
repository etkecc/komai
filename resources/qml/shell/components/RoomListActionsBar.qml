// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import im.nheko

Pane {
    id: roomActionsBar

    required property int avatarSize
    required property var componentCatalog
    required property var profileContextMenu
    required property var timelineRoot
    property int buttonSize: Math.min(30, avatarSize)
    property bool showActionButtons: roomActionsBar.width > 160

    horizontalPadding: Nheko.paddingMedium
    verticalPadding: 0

    background: Rectangle {
        color: palette.alternateBase
    }
    contentItem: RowLayout {
        spacing: Nheko.paddingMedium

        UserSettingsFlipButton {
            id: userSettingsButton

            profile: Nheko.currentUser
            avatarButtonSize: Nheko.barIconSize

            Layout.preferredHeight: Nheko.navigationRowHeight
            Layout.preferredWidth: effectiveButtonSize
            onLeftClicked: {
                if (!roomActionsBar.showActionButtons)
                    profileContextMenu.popup(userSettingsButton);
                else
                    MainWindow.showUserSettingsPage();
            }

            onRightClicked: profileContextMenu.popup(userSettingsButton)
        }
        Item {
            Layout.fillWidth: true
            visible: roomActionsBar.showActionButtons
        }
        RoomListActionButton {
            id: startChatButton

            buttonSize: roomActionsBar.buttonSize
            toolTipText: qsTr("Start a new chat")
            iconSource: ":/icons/icons/ui/plus-circle.svg"
            visible: roomActionsBar.showActionButtons

            onClicked: roomJoinCreateMenu.popup(startChatButton)
        }
        RoomJoinCreateMenu {
            id: roomJoinCreateMenu

            profileContextMenu: roomActionsBar.profileContextMenu
        }
        RoomListActionButton {
            buttonSize: roomActionsBar.buttonSize
            toolTipText: qsTr("Room directory")
            iconSource: ":/icons/icons/ui/room-directory.svg"
            visible: roomActionsBar.showActionButtons

            onClicked: profileContextMenu.openRoomDirectoryDialog()
        }
        RoomListActionButton {
            buttonSize: roomActionsBar.buttonSize
            toolTipText: qsTr("Find & switch room (Ctrl+K)")
            iconSource: ":/icons/icons/ui/search.svg"
            rippleEnabled: false
            visible: roomActionsBar.showActionButtons

            onClicked: timelineRoot.openCatalogDialog(componentCatalog.navigationQuickSwitcherDialog)
        }
    }
}
