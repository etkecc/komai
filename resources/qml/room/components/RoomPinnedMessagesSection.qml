// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai
import "../../delegates"

ScrollView {
    id: pinnedMessages

    required property var room
    required property string roomId
    readonly property bool layoutVisible: !!room && room.pinnedMessages.length > 0 && !Settings.hiddenPins.includes(roomId)

    Layout.minimumHeight: 0
    Layout.preferredHeight: layoutVisible ? Math.min(contentHeight, Komai.avatarSize * 4) : 0
    Layout.maximumHeight: layoutVisible ? Komai.avatarSize * 4 : 0
    ScrollBar.horizontal.visible: false
    clip: true
    visible: layoutVisible
    contentWidth: availableWidth

    ListView {
        model: room ? room.pinnedMessages : undefined
        spacing: Komai.paddingSmall

        delegate: RowLayout {
            required property string modelData

            height: implicitHeight
            width: ListView.view.width

            Reply {
                id: reply

                property var e: room ? room.getDump(modelData, "pins") : {}
                property string replyUserId: (e && e.userId) ? String(e.userId) : ""
                property bool isReplyFromCurrentUser: {
                    const currentUser = Komai.currentUser;
                    const currentUserId = (currentUser && currentUser.userid)
                            ? String(currentUser.userid)
                            : "";
                    return currentUserId.length > 0 && replyUserId === currentUserId;
                }

                maxWidth: pinnedMessages.width - 16
                eventId: e.eventId ?? ""
                userColor: isReplyFromCurrentUser
                    ? Komai.theme.userColorSelf
                    : room ? TimelineManager.roomUserColor(room.roomId, replyUserId, palette.window, Settings.timelineUserColorCodingPolicy) : TimelineManager.userColor(replyUserId, palette.window)
                roomColor: isReplyFromCurrentUser
                    ? Komai.theme.userColorSelf
                    : room ? TimelineManager.roomUserColor(room.roomId, replyUserId, palette.base, Settings.timelineUserColorCodingPolicy) : TimelineManager.userColor(replyUserId, palette.base)

                Connections {
                    function onPinnedMessagesChanged() {
                        reply.e = room.getDump(modelData, "pins");
                    }

                    target: room
                }
            }
            ImageButton {
                id: deletePinButton

                Layout.alignment: Qt.AlignTop | Qt.AlignRight
                Layout.preferredHeight: 16
                Layout.preferredWidth: 16
                ToolTip.text: qsTr("Unpin")
                ToolTip.visible: hovered
                hoverEnabled: true
                image: ":/icons/icons/ui/dismiss.svg"
                visible: room.permissions.canChange(MtxEvent.PinnedEvents)

                onClicked: room.unpin(modelData)
            }
        }
    }
}
