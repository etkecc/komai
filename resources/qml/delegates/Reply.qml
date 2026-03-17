// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Window
import cc.etke.komai

AbstractButton {
    id: r

    property color userColor: "red"
    property color roomColor: userColor
    property bool keepFullText: false

    required property string eventId

    property var room_: (typeof room !== "undefined") ? room : null
    property var timelineView_: (typeof timelineView !== "undefined") ? timelineView : null

    property string userId: (eventId && room_) ? room_.dataById(eventId, Room.UserId, "") : ""
    property string userName: (eventId && room_) ? room_.dataById(eventId, Room.UserName, "") : ""
    implicitHeight: replyContainer.height + topPadding + bottomPadding
    implicitWidth: replyContainer.implicitWidth + leftPadding + rightPadding

    leftPadding: 4 + Komai.paddingMedium
    rightPadding: Komai.paddingMedium
    topPadding: Komai.paddingMedium
    bottomPadding: Komai.paddingMedium

    required property int maxWidth
    property bool limitHeight: false

    KomaiCursorShape {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
    }

    onClicked: {
        let link = timelineEvent.main.linkAt != undefined && timelineEvent.main.linkAt(pressX-colorline.width, pressY - userName_.implicitHeight);
        if (link) {
            Komai.openLink(link)
        } else {
            if (room_)
                room_.showEvent(r.eventId)
        }
    }
    onPressAndHold: replyContextMenu.show(timelineEvent.main.copyText, timelineEvent.main.linkAt(pressX-colorline.width, pressY - userName_.implicitHeight), r.eventId)

    // qmllint disable required
    contentItem: TimelineEvent {
        id: timelineEvent

        isStateEvent: false
        room: r.room_
        eventId: r.eventId
        replyTo: ""
        mainInset: 4 + Komai.paddingMedium
        maxWidth: r.maxWidth
        limitAsReply: true

        data: Column {
            id: replyContainer
            spacing: 0

            clip: r.limitHeight

            height: r.limitHeight ? Math.min(timelineEvent.main?.height ?? 0, (timelineView_ ? timelineView_.height : Screen.height) / 10) + usernameBtn.height : undefined

            AbstractButton {
                id: usernameBtn

                topPadding: 0
                bottomPadding: 0
                topInset: 0
                bottomInset: 0
                height: (visible && timelineEvent.main && timelineEvent.main.y > 0) ? implicitHeight : 0

                contentItem: Label {
                    id: userName_
                    // HACK: To ensure the username gets rendered in newer Qt,
                    // we need to always have some text in here. The name should
                    // never be empty, since it falls back to the mxid, but if
                    // we have no text there, Qt culls the item before we fill it.
                    text: r.userName || "."
                    color: Komai.readableAccentTextColor(r.userColor, r.roomColor)
                    textFormat: Text.RichText
                    width: timelineEvent.main?.width
                }
                onClicked: {
                    if (room_)
                        room_.openUserProfile(r.userId);
                }
            }

            data: [
                usernameBtn, timelineEvent.main,
            ]
        }

    }
    // qmllint enable required

    background: Rectangle {
        id: backgroundItem

        z: -1
        color: r.roomColor
        radius: Komai.paddingMedium
        clip: true

        Rectangle {
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.left: parent.left

            id: colorline
            color: r.roomColor
            width: 4
            radius: parent.radius
        }
    }

}
