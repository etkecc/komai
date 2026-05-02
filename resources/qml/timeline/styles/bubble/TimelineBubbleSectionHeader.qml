// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Window
import cc.etke.komai

import "../../../components"

Column {
    readonly property int colorRevision: TimelineManager.colorRevision

    required property var day
    required property bool isSender
    required property bool isStateEvent
    required property int parentWidth
    required property var roomRef
    required property string colorRoomId
    required property var previousMessageDay
    required property var previousMessageTimestamp
    required property bool previousMessageIsStateEvent
    required property string previousMessageUserId
    required property date timestamp
    required property string userId
    required property string userName
    required property string userPowerlevel

    property int oneHour: 60 * 60 * 1000
    property bool dayBoundaryChanged: previousMessageDay !== day
    property bool showLabel: dayBoundaryChanged || timestamp - previousMessageTimestamp > oneHour
    property bool shouldShowSenderUsername: Settings.timelineMessagesLayoutAvatarSize === Settings.AvatarSize.Hidden
        ? true
        : Settings.timelineMessagesSenderUsername === 0
            ? true
            : Settings.timelineMessagesSenderUsername === 2
                ? false
                : (roomRef
                   ? roomRef.roomMemberCount
                       > Settings.timelineMessagesSenderUsernameLargeRoomThreshold
                   : false)

    bottomPadding: showLabel ? Komai.paddingMedium : (isSender ? 0 : 2)
    spacing: 8
    topPadding: userName_.visible ? 4 : 0
    visible: previousMessageUserId !== userId || showLabel || isStateEvent !== previousMessageIsStateEvent
    width: parentWidth

    Label {
        id: dateBubble

        anchors.horizontalCenter: parent ? parent.horizontalCenter : undefined
        color: palette.text
        font.pointSize: Settings.uiFontSizePt
        height: Math.round(fontMetrics.height * 1.4)
        horizontalAlignment: Text.AlignHCenter
        text: roomRef ? (dayBoundaryChanged ? roomRef.formatDateSeparator(timestamp) : roomRef.formatLaterSeparator(previousMessageTimestamp, timestamp)) : ""
        verticalAlignment: Text.AlignVCenter
        visible: roomRef && showLabel
        width: contentWidth * 1.2

        background: Rectangle {
            readonly property string _threadEventId: TimelineManager.matrixTimelineThreadEventId
            readonly property bool _threadActive: _threadEventId.length > 0
            readonly property color _threadTintColor: _threadActive
                ? TimelineManager.userColor(_threadEventId, palette.base)
                : palette.buttonText
            color: _threadActive
                ? Qt.tint(palette.window, Qt.hsla(_threadTintColor.hslHue, 0.7,
                                                  _threadTintColor.hslLightness, 0.1))
                : palette.window
            radius: parent.height / 2
        }
    }
    Row {
        id: userInfo

        property int remainingWidth: chat.delegateMaxWidth

        height: userName_.height
        spacing: 4
        visible: !isStateEvent && shouldShowSenderUsername && (!isSender || Settings.timelineMessagesLayoutAvatarSize === Settings.AvatarSize.Hidden)

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

                permissions: roomRef ? roomRef.permissions : null
                visible: isAdmin || isModerator
            }

            leftPadding: powerlevelIndicator.visible ? 16 : 0
            leftInset: 0
            rightInset: 0
            rightPadding: 0

            KomaiToolTip {
                anchorItem: userNameButton
                anchorX: userNameButton.width / 2
                anchorY: 0
                text: userId
                delay: Komai.tooltipDelay
                requestedVisible: userNameButton.hovered
            }

            contentItem: Label {
                id: userName_

                color: Komai.readableAccentTextColor(
                    (function() {
                        const _revision = colorRevision;
                        return colorRoomId ? TimelineManager.roomUserColor(colorRoomId, userId, palette.base, Settings.timelineUserColorCodingPolicy)
                                           : TimelineManager.userColor(userId, palette.base);
                    })(),
                    palette.base)
                font.pointSize: Settings.uiFontSizePt
                text: TimelineManager.escapeEmoji(userNameTextMetrics.elidedText)
                textFormat: Text.RichText
            }

            onClicked: {
                if (roomRef && roomRef.roomId && userId)
                    TimelineManager.openRoomUserProfile(roomRef.roomId, userId)
            }

            TextMetrics {
                id: userNameTextMetrics

                elide: Text.ElideRight
                elideWidth: userInfo.remainingWidth - Math.min(statusMsg.implicitWidth, userInfo.remainingWidth / 3)
                text: userName
            }
            KomaiCursorShape {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
            }
        }
        Label {
            id: statusMsg

            property string userStatus: Presence.userStatus(userId)

            anchors.baseline: userNameButton.baseline
            color: palette.buttonText
            elide: Text.ElideRight
            font.italic: true
            font.pointSize: Math.floor(Settings.uiFontSizePt * 0.8)
            text: userStatus.replace(/\n/g, " ")
            textFormat: Text.PlainText
            width: Math.min(implicitWidth, userInfo.remainingWidth - userName_.width - parent.spacing)

            HoverHandler {
                id: statusMsgHoverHandler
            }

            KomaiToolTip {
                anchorItem: statusMsg
                anchorX: statusMsg.width / 2
                anchorY: 0
                text: qsTr("%1's status message").arg(userName)
                delay: Komai.tooltipDelay
                requestedVisible: statusMsgHoverHandler.hovered
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
