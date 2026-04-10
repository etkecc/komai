// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import cc.etke.komai

Menu {
    id: root

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
        text: qsTr("App Settings")
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
        text: qsTr("Sign out")
        icon.source: "qrc:/icons/icons/ui/power-off.svg"
        onTriggered: Komai.openLogoutDialog()
    }
}
