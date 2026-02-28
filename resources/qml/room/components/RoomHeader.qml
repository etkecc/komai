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
    property var roomModel: room
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
                room: topBar.roomModel
                lineSpacing: fontMetrics.lineSpacing
            }
            RoomHeaderBackButton {
                id: backToRoomsButton

                topBarRef: topBar
                column: 0
                showBackButton: topBar.showBackButton
            }
            RoomHeaderRoomAvatar {
                room: topBar.roomModel
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
            RoomHeaderPinButton {
                id: pinButton

                topBarRef: topBar
                column: 4
                room: topBar.roomModel
                roomId: topBar.roomId
            }
            RoomHeaderSearchButton {
                id: searchButton

                topBarRef: topBar
                column: 5
                room: topBar.roomModel

                onSearchActiveChanged: {
                    if (searchActive) {
                        roomSearchRow.focusInput();
                    } else {
                        roomSearchRow.clearInput();
                        topBar.searchString = "";
                    }
                }
            }
            RoomHeaderMembersButton {
                id: memberButton

                topBarRef: topBar
                column: 6
                room: topBar.roomModel
            }
            RoomEncryptionStatusButton {
                isEncrypted: topBar.isEncrypted
                roomAvailable: !!room
                trustlevel: topBar.trustlevel
                topBarAvatarSize: topBar.topBarAvatarSize
                buttonPaddingH: topBar.buttonPaddingH
                buttonPaddingV: topBar.buttonPaddingV
            }
            RoomHeaderSettingsButton {
                topBarRef: topBar
                column: 8
                roomAvailable: !!topBar.roomModel
                roomId: topBar.roomId
            }
            RoomOptionsButton {
                roomAvailable: !!topBar.roomModel
                roomId: topBar.roomId
                topBarAvatarSize: topBar.topBarAvatarSize
                buttonPaddingH: topBar.buttonPaddingH
                buttonPaddingV: topBar.buttonPaddingV
            }
            RoomPinnedMessagesSection {
                room: topBar.roomModel
                roomId: topBar.roomId
            }
            RoomWidgetsSection {
                room: topBar.roomModel
                roomId: topBar.roomId
            }
            RoomHeaderSearchRow {
                id: roomSearchRow

                room: topBar.roomModel
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
