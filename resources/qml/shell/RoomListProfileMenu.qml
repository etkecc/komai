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
        timelineRoot.openCatalogDialog(componentCatalog.roomCreateDialog, properties || {});
    }
    function openCreateDirectDialog() {
        timelineRoot.openCatalogDialog(componentCatalog.roomCreateDirectDialog);
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
        acceptText: qsTr("Set")
        text: profile ? Presence.userStatus(profile.userid) : ""
        onInputAccepted: function (text) {
            Komai.setStatusMessage(text);
        }
    }

    MenuItem {
        text: qsTr("Profile Settings")
        icon.source: "qrc:/icons/icons/ui/person.svg"
        onTriggered: MainWindow.showUserSettingsPage(UserSettingsModel.TabAccount)
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
        text: qsTr("Open Profile Switcher")
        icon.source: "qrc:/icons/icons/ui/people.svg"
        onTriggered: {
            const error = Komai.launchProfileSwitcher();
            if (error.length > 0)
                console.error("Failed to launch profile switcher:", error);
        }
    }
    MenuSeparator {
    }
    MenuItem {
        text: qsTr("Join room")
        icon.source: "qrc:/icons/icons/ui/plus-circle.svg"
        onTriggered: Komai.openJoinRoomDialog()
    }
    MenuSeparator {}
    MenuItem {
        text: qsTr("New room")
        icon.source: "qrc:/icons/icons/ui/plus-circle.svg"
        onTriggered: root.openCreateRoomDialog({})
    }
    MenuItem {
        text: qsTr("New direct chat")
        icon.source: "qrc:/icons/icons/ui/plus-circle.svg"
        onTriggered: root.openCreateDirectDialog()
    }
    MenuSeparator {}
    MenuItem {
        text: qsTr("New space")
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
