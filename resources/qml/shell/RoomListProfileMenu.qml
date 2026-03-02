// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import cc.etke.komai

Menu {
    id: root

    required property var timelineRoot
    required property var componentCatalog

    function openCurrentUserProfile() {
        Komai.updateUserProfile();
        timelineRoot.showCatalogDialog(componentCatalog.userProfileDialog, {
                "profile": Komai.currentUser
            });
    }
    function openCreateRoomDialog(properties) {
        timelineRoot.showCatalogDialog(componentCatalog.roomCreateDialog, properties || {});
    }
    function openCreateDirectDialog() {
        timelineRoot.showCatalogDialog(componentCatalog.roomCreateDirectDialog);
    }
    function openRoomDirectoryDialog() {
        timelineRoot.showCatalogDialog(componentCatalog.roomDirectoryDialog);
    }

    Component.onCompleted: {
        if (root.popupType != undefined)
            root.popupType = 2;
    }

    InputDialog {
        id: profileStatusDialog

        property var profile: Komai.currentUser

        prompt: qsTr("Enter your status message:")
        title: qsTr("Status Message")
        titleIcon: ":/icons/icons/ui/tag.svg"
        text: profile ? Presence.userStatus(profile.userid) : ""
        onInputAccepted: function (text) {
            Komai.setStatusMessage(text);
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
        onTriggered: profileStatusDialog.open()
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
        onTriggered: Komai.openJoinRoomDialog()
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
        onTriggered: Komai.openLogoutDialog()
    }
}
