// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.2
import im.nheko 1.0
import "../../delegates"
import "../../ui"

Pane {
    id: topBar

    property string avatarUrl: room ? room.roomAvatarUrl : ""
    property string directChatOtherUserId: room ? room.directChatOtherUserId : ""
    property bool isDirect: room ? room.isDirect : false
    property bool isEncrypted: room ? room.isEncrypted : false
    property string roomId: room ? room.roomId : ""
    property string roomName: room ? room.roomName : qsTr("No room selected")
    property string roomTopic: room ? room.roomTopic : ""
    property bool searchHasFocus: roomSearchRow.searchHasFocus
    property string searchString: ""
    property bool showBackButton: false
    property bool filteringInProgress: false
    property bool filterNotifications: false
    property int trustlevel: room ? room.trustlevel : Crypto.Unverified
    property int topBarAvatarSize: Nheko.barIconSize
    property int buttonPaddingH: Nheko.uiLayoutCompactMode ? Nheko.paddingSmall : Nheko.paddingMedium
    property int buttonPaddingV: 0

    Layout.fillWidth: true
    Layout.minimumHeight: Nheko.uiLayoutCompactMode ? Nheko.navigationRowHeight : 0
    implicitHeight: Math.max(topLayout.height + Nheko.paddingMedium * 2, Nheko.navigationRowHeight)
    padding: 0
    z: 3

    background: Rectangle {
        color: palette.alternateBase
    }
    contentItem: Item {
    GridLayout {
        id: topLayout

        anchors.left: parent.left
        anchors.leftMargin: Nheko.paddingMedium
        anchors.right: parent.right
        anchors.rightMargin: Nheko.paddingMedium
        anchors.top: parent.top
        columnSpacing: 0
            rowSpacing: Nheko.uiLayoutCompactMode ? 0 : Nheko.paddingSmall

            RoomHeaderCommunitySection {
                room: topBar.room
                lineSpacing: fontMetrics.lineSpacing
            }
            RoomHeaderActionButton {
                id: backToRoomsButton

                topBarRef: topBar
                column: 0
                ToolTip.text: qsTr("Back to room list")
                ToolTip.visible: hovered
                image: ":/icons/icons/ui/angle-arrow-left.svg"
                visible: showBackButton

                onClicked: Rooms.resetCurrentRoom()
            }
            RoomHeaderRoomAvatar {
                room: topBar.room
                roomId: topBar.roomId
                avatarUrl: topBar.avatarUrl
                isDirect: topBar.isDirect
                directChatOtherUserId: topBar.directChatOtherUserId
                topBarAvatarSize: topBar.topBarAvatarSize
                buttonPaddingH: topBar.buttonPaddingH
            }
            RoomHeaderRoomNameLabel {
                roomName: topBar.roomName
            }
            RoomHeaderTopicText {
                roomTopic: topBar.roomTopic
                compactMode: Nheko.uiLayoutCompactMode
                lineSpacing: fontMetrics.lineSpacing
            }
            // BROKEN: "Show only notifications" filter doesn't work properly.
            // It only filters messages already loaded in QML, not the full timeline.
            // The virtual timeline window (commit 5b47f5c6) makes this worse by capping
            // exposed messages to 200, but the feature was broken even before that.
            // Fixing would likely require scanning the database for highlighted messages.
            // Hiding for now until we can revisit this feature.
            // ImageButton {
            //     id: notificationsButton
            //
            //     Layout.alignment: Qt.AlignRight
            //     Layout.column: 3
            //     Layout.preferredHeight: Nheko.avatarSize - Nheko.paddingMedium
            //     Layout.preferredWidth: Nheko.avatarSize - Nheko.paddingMedium
            //     Layout.row: 1
            //     ToolTip.text: qsTr("Show only notifications")
            //     ToolTip.visible: hovered
            //     image: ":/icons/icons/ui/alert.svg"
            //
            //     onClicked: {
            //         topBar.filterNotifications = !topBar.filterNotifications
            //     }
            // }
            RoomHeaderActionButton {
                id: pinButton

                property bool pinsShown: !Settings.hiddenPins.includes(roomId)

                topBarRef: topBar
                column: 4
                ToolTip.text: qsTr("Show or hide pinned messages")
                ToolTip.visible: hovered
                image: pinsShown ? ":/icons/icons/ui/pin.svg" : ":/icons/icons/ui/pin-off.svg"
                visible: !!room && room.pinnedMessages.length > 0

                onClicked: {
                    var ps = Settings.hiddenPins;
                    if (pinsShown) {
                        ps.push(roomId);
                    } else {
                        const index = ps.indexOf(roomId);
                        if (index > -1) {
                            ps.splice(index, 1);
                        }
                    }
                    Settings.hiddenPins = ps;
                }
            }
            RoomHeaderActionButton {
                id: searchButton

                property bool searchActive: false

                topBarRef: topBar
                column: 5
                ToolTip.text: qsTr("Search this room")
                ToolTip.visible: hovered
                image: ":/icons/icons/ui/search.svg"
                visible: !!room

                onClicked: searchActive = !searchActive
                onSearchActiveChanged: {
                    if (searchActive) {
                        roomSearchRow.focusInput();
                    } else {
                        roomSearchRow.clearInput();
                        topBar.searchString = "";
                    }
                }
            }
            RoomHeaderActionButton {
                id: memberButton

                topBarRef: topBar
                column: 6
                visible: !!room

                ToolTip.text: qsTr("Show room members.")
                ToolTip.visible: hovered
                image: ":/icons/icons/ui/people.svg"

                onClicked: TimelineManager.openRoomMembers(room)
            }
            RoomEncryptionStatusButton {
                isEncrypted: topBar.isEncrypted
                roomAvailable: !!room
                trustlevel: topBar.trustlevel
                topBarAvatarSize: topBar.topBarAvatarSize
                buttonPaddingH: topBar.buttonPaddingH
                buttonPaddingV: topBar.buttonPaddingV
            }
            RoomHeaderActionButton {
                id: roomSettingsButton

                topBarRef: topBar
                column: 8
                ToolTip.text: qsTr("Room settings")
                ToolTip.visible: hovered
                image: ":/icons/icons/ui/toggles.svg"
                visible: !!room

                onClicked: TimelineManager.openRoomSettings(roomId)
            }
            RoomOptionsButton {
                roomAvailable: !!room
                roomId: topBar.roomId
                topBarAvatarSize: topBar.topBarAvatarSize
                buttonPaddingH: topBar.buttonPaddingH
                buttonPaddingV: topBar.buttonPaddingV
            }
            RoomPinnedMessagesSection {
                room: topBar.room
                roomId: topBar.roomId
            }
            RoomWidgetsSection {
                room: topBar.room
                roomId: topBar.roomId
            }
            RoomHeaderSearchRow {
                id: roomSearchRow

                room: topBar.room
                filteringInProgress: topBar.filteringInProgress
                topBarAvatarSize: topBar.topBarAvatarSize
                searchActive: searchButton.searchActive

                onSearchStringCommitted: function (value) {
                    topBar.searchString = value;
                }
                onRequestClose: searchButton.searchActive = false
            }
        }
    }

    onRoomIdChanged: {
        searchString = "";
        searchButton.searchActive = false;
        filterNotifications = false;
    }

    Shortcut {
        sequence: StandardKey.Find

        onActivated: searchButton.searchActive = !searchButton.searchActive
    }
}
