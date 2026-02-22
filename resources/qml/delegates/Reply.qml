// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Window
import im.nheko
import "../"

AbstractButton {
    id: r

    property color userColor: "red"
    property color roomColor: userColor
    property bool keepFullText: false

    required property string eventId

    property var room_: room
    property bool replyEventIsStateEvent: eventId ? room.dataById(eventId, Room.IsStateEvent, false) : false
    property int replyEventType: eventId ? room.dataById(eventId, Room.Type, MtxEvent.UnknownMessage) : MtxEvent.UnknownMessage
    property string replyEventTypeString: eventId ? room.dataById(eventId, Room.TypeString, "") : ""
    property string replyEventBody: eventId ? room.dataById(eventId, Room.Body, "") : ""
    property string replyEventFormattedBody: eventId ? room.dataById(eventId, Room.FormattedBody, "") : ""
    property string replyEventFormattedStateEvent: eventId ? room.dataById(eventId, Room.FormattedStateEvent, "") : ""
    property string replyEventCallType: eventId ? room.dataById(eventId, Room.CallType, "") : ""

    property string userId: eventId ? room.dataById(eventId, Room.UserId, "") : ""
    property string userName: eventId ? room.dataById(eventId, Room.UserName, "") : ""
    implicitHeight: replyContainer.height + topPadding + bottomPadding
    implicitWidth: replyContainer.implicitWidth + leftPadding + rightPadding

    leftPadding: 4 + Nheko.paddingMedium
    rightPadding: Nheko.paddingMedium
    topPadding: Nheko.paddingMedium
    bottomPadding: Nheko.paddingMedium

    required property int maxWidth
    property bool limitHeight: false

    NhekoCursorShape {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
    }

    onClicked: {
        let link = timelineEvent.main.linkAt != undefined && timelineEvent.main.linkAt(pressX-colorline.width, pressY - userName_.implicitHeight);
        if (link) {
            Nheko.openLink(link)
        } else {
            room.showEvent(r.eventId)
        }
    }
    onPressAndHold: replyContextMenu.show(timelineEvent.main.copyText, timelineEvent.main.linkAt(pressX-colorline.width, pressY - userName_.implicitHeight), r.eventId)

    // qmllint disable required
    contentItem: TimelineEvent {
        id: timelineEvent

        isStateEvent: r.replyEventIsStateEvent
        room: r.room_
        eventId: r.eventId
        type: r.replyEventType
        typeString: r.replyEventTypeString
        body: r.replyEventBody
        formattedBody: r.replyEventFormattedBody
        formattedStateEvent: r.replyEventFormattedStateEvent
        callType: r.replyEventCallType
        replyTo: ""
        mainInset: 4 + Nheko.paddingMedium
        maxWidth: r.maxWidth
        limitAsReply: r.limitHeight

        data: Column {
            id: replyContainer
            spacing: 0

            clip: r.limitHeight

            height: r.limitHeight ? Math.min( timelineEvent.main?.height, timelineView.height / 10) + usernameBtn.height : undefined

            // FIXME: I have no idea, why this name doesn't render in the reply popup on Qt 6.9.2
            AbstractButton {
                id: usernameBtn

                topPadding: 0
                bottomPadding: 0
                topInset: 0
                bottomInset: 0
                visible: r.eventId
                height: (visible && timelineEvent.main && timelineEvent.main.y > 0) ? implicitHeight : 0

                contentItem: Label {
                    visible: r.eventId
                    id: userName_
                    text: r.userName
                    color: Qt.darker(r.userColor, 1.3)
                    textFormat: Text.RichText
                    width: timelineEvent.main?.width
                }
                onClicked: room.openUserProfile(r.userId)
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
        property color bgColor: palette.base
        color: Qt.tint(bgColor, Qt.hsla(r.roomColor.hslHue, 0.5, r.roomColor.hslLightness, 0.1))
        radius: Nheko.paddingMedium
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
