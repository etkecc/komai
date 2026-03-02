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
    property string roomid
    property var tags

    function show(menuParent, roomid_, tags_) {
        roomid = roomid_;
        tags = tags_;
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

        onInputAccepted: function (text) {
            Rooms.toggleTag(root.roomid, "u." + text, true);
        }
    }
    MenuItem {
        text: qsTr("Open separately")

        onTriggered: {
            var roomWindow = root.roomWindowComponent.createObject(null, {
                    "room": Rooms.getRoomById(root.roomid),
                    "roomPreview": Rooms.getRoomPreviewById(root.roomid)
                });
            roomWindow.showNormal();
            root.timelineRoot.destroyOnClose(roomWindow);
        }
    }
    MenuItem {
        text: qsTr("Mark as read")

        onTriggered: Rooms.getRoomById(root.roomid).markRoomAsRead()
    }
    MenuItem {
        text: qsTr("Room settings")

        onTriggered: TimelineManager.openRoomSettings(root.roomid)
    }
    MenuItem {
        text: qsTr("Leave room")

        onTriggered: TimelineManager.openLeaveRoomDialog(root.roomid)
    }
    MenuItem {
        text: qsTr("Copy room link")

        onTriggered: Rooms.copyLink(root.roomid)
    }
    Menu {
        id: tagsMenu

        title: qsTr("Tag room as:")

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
    SpaceMenu {
        roomid: root.roomid
    }
}
