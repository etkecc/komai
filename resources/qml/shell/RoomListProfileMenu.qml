// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../dialogs/common"
import QtQuick
import QtQuick.Controls
import im.nheko

Menu {
    id: root

    required property var timelineRoot
    required property var componentCatalog
    required property var createRoomComponent
    required property var createDirectComponent
    required property var roomDirectoryComponent

    function openCurrentUserProfile() {
        Nheko.updateUserProfile();
        timelineRoot.showCatalogDialog(componentCatalog.userProfileDialog, {
                "profile": Nheko.currentUser
            });
    }
    function openCreateRoomDialog(properties) {
        var createRoom = createRoomComponent.createObject(timelineRoot, properties || {});
        createRoom.show();
        timelineRoot.destroyOnClose(createRoom);
    }
    function openCreateDirectDialog() {
        var createDirect = createDirectComponent.createObject(timelineRoot);
        createDirect.show();
        timelineRoot.destroyOnClose(createDirect);
    }
    function openRoomDirectoryDialog() {
        var win = roomDirectoryComponent.createObject(timelineRoot);
        win.show();
        timelineRoot.destroyOnClose(win);
    }

    Component.onCompleted: {
        if (root.popupType != undefined)
            root.popupType = 2;
    }

    InputDialog {
        id: profileStatusDialog

        property var profile: Nheko.currentUser

        prompt: qsTr("Enter your status message:")
        title: qsTr("Status Message")
        text: profile ? Presence.userStatus(profile.userid) : ""
        onAccepted: function (text) {
            Nheko.setStatusMessage(text);
        }
    }

    MenuItem {
        text: qsTr("Profile Settings")
        icon.source: "qrc:/icons/icons/ui/person.svg"
        onTriggered: root.openCurrentUserProfile()
    }
    MenuItem {
        text: qsTr("Set Status Message")
        icon.source: "qrc:/icons/icons/ui/tag.svg"
        onTriggered: profileStatusDialog.show()
    }
    MenuSeparator {
    }
    MenuItem {
        text: qsTr("Application Settings")
        icon.source: "qrc:/icons/icons/ui/toggles.svg"
        onTriggered: MainWindow.showUserSettingsPage()
    }
    MenuSeparator {
    }
    MenuItem {
        text: qsTr("Join a room")
        icon.source: "qrc:/icons/icons/ui/plus-circle.svg"
        onTriggered: Nheko.openJoinRoomDialog()
    }
    MenuItem {
        text: qsTr("Create a new room")
        icon.source: "qrc:/icons/icons/ui/plus-circle.svg"
        onTriggered: root.openCreateRoomDialog({})
    }
    MenuItem {
        text: qsTr("Start a direct chat")
        icon.source: "qrc:/icons/icons/ui/plus-circle.svg"
        onTriggered: root.openCreateDirectDialog()
    }
    MenuItem {
        text: qsTr("Create a new community")
        icon.source: "qrc:/icons/icons/ui/plus-circle.svg"
        onTriggered: root.openCreateRoomDialog({
                "space": true
            })
    }
    MenuItem {
        text: qsTr("Room directory")
        icon.source: "qrc:/icons/icons/ui/room-directory.svg"
        onTriggered: root.openRoomDirectoryDialog()
    }
    MenuSeparator {
    }
    MenuItem {
        text: qsTr("Logout")
        icon.source: "qrc:/icons/icons/ui/power-off.svg"
        onTriggered: Nheko.openLogoutDialog()
    }
}
