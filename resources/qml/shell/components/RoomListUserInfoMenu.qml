// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import cc.etke.komai

Item {
    id: root

    required property var profileContextMenu
    required property var profile

    function close() {
        contextMenu.close();
    }

    function popup(menuParent) {
        contextMenu.popup(menuParent);
    }

    function openUserProfile() {
        profileContextMenu.openCurrentUserProfile();
    }

    InputDialog {
        id: statusDialog

        prompt: qsTr("Enter your status message:")
        title: qsTr("Status Message")
        titleIcon: ":/icons/icons/ui/tag.svg"
        text: root.profile ? Presence.userStatus(root.profile.userid) : ""

        onInputAccepted: function (text) {
            Komai.setStatusMessage(text);
        }
    }

    Menu {
        id: contextMenu

        MenuItem {
            text: qsTr("Profile settings")

            onTriggered: root.openUserProfile()
        }
        MenuItem {
            text: qsTr("Set status message")

            onTriggered: statusDialog.open()
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
}
