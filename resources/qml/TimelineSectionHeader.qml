// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Window
import im.nheko

import "./components"

Column {

    required property var day
    required property bool isSender
    required property bool isStateEvent
    required property int parentWidth
    required property var previousMessageDay
    required property var previousMessageTimestamp
    required property bool previousMessageIsStateEvent
    required property string previousMessageUserId
    required property date timestamp
    required property string userId
    required property string userName
    required property string userPowerlevel

    property int oneHour: 60 * 60 * 1000
    property bool dayChanged: previousMessageDay !== day
    property bool showLabel: dayChanged || timestamp - previousMessageTimestamp > oneHour
    property bool shouldShowSenderUsername: Settings.timelineMessagesSenderUsername === 0 ? true : Settings.timelineMessagesSenderUsername === 2 ? false : (room ? room.roomMemberCount > Settings.timelineMessagesSenderUsernameLargeRoomThreshold : true)

    bottomPadding: Settings.timelineMessageLayout === Settings.TimelineMessageLayout.Bubbles ? (isSender && !showLabel ? 0 : 2) : 3
    spacing: 8
    topPadding: userName_.visible ? 4 : 0
    visible: (previousMessageUserId !== userId || showLabel || isStateEvent !== previousMessageIsStateEvent)
    width: parentWidth

    Label {
        id: dateBubble

        anchors.horizontalCenter: parent ? parent.horizontalCenter : undefined
        color: palette.text
        height: Math.round(fontMetrics.height * 1.4)
        horizontalAlignment: Text.AlignHCenter
        text: room ? (dayChanged ? room.formatDateSeparator(timestamp) : room.formatLaterSeparator(previousMessageTimestamp, timestamp)) : ""
        verticalAlignment: Text.AlignVCenter
        visible: room && showLabel
        width: contentWidth * 1.2

        background: Rectangle {
            color: palette.window
            radius: parent.height / 2
        }
    }
    Row {
        id: userInfo

        property int remainingWidth: chat.delegateMaxWidth

        height: userName_.height
        spacing: 4
        visible: !isStateEvent && shouldShowSenderUsername && (Settings.timelineMessageLayout === Settings.TimelineMessageLayout.Bubbles ? !isSender : true)

        AbstractButton {
            id: userNameButton

            PowerlevelIndicator {
                id: powerlevelIndicator
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter

                powerlevel: userPowerlevel
                height: fontMetrics.ascent
                width: height

                sourceSize.width: width
                sourceSize.height: height

                permissions: room ? room.permissions : null
                visible: isAdmin || isModerator
            }

            ToolTip.delay: Nheko.tooltipDelay
            ToolTip.text: userId
            ToolTip.visible: hovered
            leftPadding: powerlevelIndicator.visible ? 16 : 0
            leftInset: 0
            rightInset: 0
            rightPadding: 0

            contentItem: Label {
                id: userName_

                color: Qt.darker(room ? TimelineManager.roomUserColor(room.roomId, userId, palette.base, palette.highlight) : TimelineManager.userColor(userId, palette.base), 1.3)
                text: TimelineManager.escapeEmoji(userNameTextMetrics.elidedText)
                textFormat: Text.RichText
            }

            onClicked: room.openUserProfile(userId)

            TextMetrics {
                id: userNameTextMetrics

                elide: Text.ElideRight
                elideWidth: userInfo.remainingWidth - Math.min(statusMsg.implicitWidth, userInfo.remainingWidth / 3)
                text: userName
            }
            NhekoCursorShape {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
            }
        }
        Label {
            id: statusMsg

            property string userStatus: Presence.userStatus(userId)

            ToolTip.delay: Nheko.tooltipDelay
            ToolTip.text: qsTr("%1's status message").arg(userName)
            ToolTip.visible: statusMsgHoverHandler.hovered
            anchors.baseline: userNameButton.baseline
            color: palette.buttonText
            elide: Text.ElideRight
            font.italic: true
            font.pointSize: Math.floor(Settings.fontSize * 0.8)
            text: userStatus.replace(/\n/g, " ")
            textFormat: Text.PlainText
            width: Math.min(implicitWidth, userInfo.remainingWidth - userName_.width - parent.spacing)

            HoverHandler {
                id: statusMsgHoverHandler

            }
            Connections {
                function onPresenceChanged(id) {
                    if (id == userId)
                    statusMsg.userStatus = Presence.userStatus(userId);
                }

                target: Presence
            }
        }
    }
}
