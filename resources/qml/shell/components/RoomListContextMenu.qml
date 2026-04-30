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
    required property var roomWindowComponent
    required property var tabController
    property string roomid
    property var tags
    property bool roomIsSpace: false
    property bool roomIsInvite: false
    property bool roomHasUnread: false

    function show(menuParent, roomid_, tags_, isSpace_, isInvite_, hasUnread_) {
        roomid = roomid_;
        tags = tags_;
        roomIsSpace = !!isSpace_;
        roomIsInvite = !!isInvite_;
        roomHasUnread = !!hasUnread_;
        popup(menuParent);
    }

    Component.onCompleted: {
        if (root.popupType != undefined)
            root.popupType = 2; // Popup.Native with fallback on older Qt (<6.8.0)
    }

    InputDialog {
        id: newTag

        prompt: qsTr("Enter the tag you want to use:")
        title: qsTr("New tag")
        titleIcon: ":/icons/icons/ui/tag.svg"
        acceptText: qsTr("Create")

        onInputAccepted: function (text) {
            Rooms.toggleTag(root.roomid, "u." + text, true);
        }
    }
    MenuItem {
        text: qsTr("Open in a new tab")
        icon.source: "qrc:/icons/icons/ui/tab-add.svg"

        onTriggered: root.tabController.openTab(root.roomid)
    }
    MenuItem {
        text: qsTr("Open in new window")
        icon.source: "qrc:/icons/icons/ui/window-new.svg"

        onTriggered: {
            var roomWindow = root.roomWindowComponent.createObject(null, {
                    "roomPreview": Rooms.getRoomPreviewById(root.roomid)
                });
            roomWindow.showNormal();
            root.timelineRoot.destroyOnClose(roomWindow);
        }
    }
    MenuSeparator {}
    MenuItem {
        text: qsTr("Copy room link")
        icon.source: "qrc:/icons/icons/ui/copy.svg"

        onTriggered: Rooms.copyLink(root.roomid)
    }
    MenuItem {
        text: qsTr("Mark as read")
        icon.source: "qrc:/icons/icons/ui/checkmark.svg"
        height: visible ? implicitHeight : 0
        visible: !root.roomIsSpace && !root.roomIsInvite && root.roomHasUnread

        onTriggered: Rooms.markAsRead(root.roomid)
    }
    Menu {
        id: tagsMenu

        title: qsTr("Tag room as:")
        icon.source: "qrc:/icons/icons/ui/tag.svg"

        Instantiator {
            model: Communities.tagsWithDefault

            delegate: MenuItem {
                property string t: modelData

                checkable: true
                checked: root.tags !== undefined && root.tags.includes(t)
                text: {
                    switch (t) {
                    case "m.favourite":
                        return qsTr("Favourite");
                    case "m.lowpriority":
                        return qsTr("Low priority");
                    case "m.server_notice":
                        return qsTr("Server notice");
                    default:
                        return t.substring(2);
                    }
                }

                onTriggered: Rooms.toggleTag(root.roomid, t, checked)
            }

            onObjectAdded: (index, object) => tagsMenu.insertItem(index, object)
            onObjectRemoved: (index, object) => tagsMenu.removeItem(object)
        }
        MenuItem {
            text: qsTr("Create new tag...")

            onTriggered: newTag.open()
        }
    }
    MenuItem {
        text: qsTr("Room settings")
        icon.source: "qrc:/icons/icons/ui/settings.svg"

        onTriggered: TimelineManager.openRoomInfo(root.roomid, "settings")
    }
    MenuSeparator {}
    MenuItem {
        text: qsTr("Leave room")
        icon.source: "qrc:/icons/icons/ui/power-off.svg"

        onTriggered: TimelineManager.openLeaveRoomDialog(root.roomid)
    }
}
