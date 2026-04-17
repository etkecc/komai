// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Window
import cc.etke.komai
import "../../../components"

TimelineMessageStyleBase {
    id: wrapper
    // We return a larger size for any item but the most bottom one, if it isn't initialized yet, since otherwise Qt will create way too many items.
    // If we did that also for the first item, it would mess with the scroll location a bit, so we don't do it for that item.
    // Events that slipped into the message index (e.g. encrypted events whose keys
    // arrived late and turned out to be reactions, edits, etc.) must be collapsed.
    visible: !isHiddenEvent
    // True once the bubble body has computed its real height (implicitHeight >= 1)
    // or for index 0 where the fallback is never used. External height estimators
    // (e.g. MatrixRoomView.sharedTimelineHeightEstimate) use this to avoid adopting
    // the 100 px placeholder while the delegate is still loading.
    readonly property bool contentReady: !isHiddenEvent && (bubbleBody.implicitHeight >= 1 || index === 0)
    property bool perfLoggedContentReady: false
    height: isHiddenEvent ? 0 : Math.max((section.item?.height ?? 0)
        + Math.max(((bubbleBody.implicitHeight < 1 && index != 0) ? 100 : bubbleBody.implicitHeight),
                   (reserveAvatarRowHeight && messageUserAvatar.visible ? messageUserAvatar.height : 0))
        + (reactionRowLoader.item?.implicitHeight ?? 0)
        + (unreadRowLoader.item?.height ?? 0),
        10)
    //room: chatRoot.roommodel
    styleProfile: TimelineStyleProfile {
        fileMessagePadding: wrapper.styleFileMessagePadding
        showFileMessageBackground: wrapper.styleShowFileMessageBackground
        showEncryptedMessageBackground: wrapper.styleShowEncryptedMessageBackground
    }

    property int styleFileMessagePadding: 8
    property bool styleShowFileMessageBackground: false
    property bool styleShowEncryptedMessageBackground: false

    property int messageBubblePadding: Komai.uiLayoutCompactMode ? Komai.paddingSmall : Komai.paddingMedium
    property int messageBubbleHorizontalPadding: messageBubblePadding
    property int messageBubbleVerticalPadding: messageBubblePadding
    property int messageBubbleRadius: 8
    property bool messageBubbleBackgroundEnabled: true
    property bool alignMessageTextToSide: false
    readonly property bool perfDisableTimelineSectionHeaders: TimelineManager.perfUiFlagEnabled("disable_timeline_section_headers")
    readonly property bool perfDisableTimelineAvatars: TimelineManager.perfUiFlagEnabled("disable_timeline_avatars")
    readonly property bool perfDisableTimelineReactions: TimelineManager.perfUiFlagEnabled("disable_timeline_reactions")
    property bool reserveAvatarRowHeight: !perfDisableTimelineAvatars && startsNewMessageGroup
    property bool pushMetadataToEdge: false
    property bool alignBubbleToTop: true

    property bool shouldShowMessageAvatar: !perfDisableTimelineAvatars
        && !wrapper.isStateEvent
        && Settings.timelineMessagesLayoutAvatarSize !== Settings.AvatarSize.Hidden
        && (!wrapper.isSender || Settings.timelineMessagesLayoutShowOwnAvatar)
    property int avatarMargin: (shouldShowMessageAvatar ? (Komai.iconSize * (Settings.timelineMessagesLayoutAvatarSize === Settings.AvatarSize.Small ? 0.5 : 1) + 8) : 0) // align with avatar
    property bool avatarIsOnRight: wrapper.messageIsRightAligned

    property alias hovered: bubbleBody.hovered
    keyboardActionAnchorItem: bubbleBody.bubbleItem
    property real selectionTintOpacity: messageBubbleBackgroundEnabled ? 0.16 : 0.22
    readonly property color selectionOutlineColor: Qt.rgba(palette.highlight.r,
                                                            palette.highlight.g,
                                                            palette.highlight.b,
                                                            0.95)
    readonly property color focusedOutlineColor: Qt.rgba(selectionOutlineColor.r,
                                                         selectionOutlineColor.g,
                                                         selectionOutlineColor.b,
                                                         0.72)
    readonly property color selectedBorderColor: Qt.rgba(selectionOutlineColor.r,
                                                         selectionOutlineColor.g,
                                                         selectionOutlineColor.b,
                                                         1.0)
    readonly property color selectionTintColor: Qt.rgba(selectionOutlineColor.r,
                                                        selectionOutlineColor.g,
                                                        selectionOutlineColor.b,
                                                        selectionTintOpacity)
    mainMessageTextColor: (bubbleBody && bubbleBody.bubbleItem && bubbleBody.bubbleItem.roomBubblePalette && bubbleBody.bubbleItem.roomBubblePalette.text !== undefined)
                          ? bubbleBody.bubbleItem.roomBubblePalette.text
                          : palette.text
    mainMessageSecondaryTextColor: (bubbleBody && bubbleBody.bubbleItem && bubbleBody.bubbleItem.roomBubblePalette && bubbleBody.bubbleItem.roomBubblePalette.buttonText !== undefined)
                                   ? bubbleBody.bubbleItem.roomBubblePalette.buttonText
                                   : palette.buttonText
    mainMessageLinkColor: (bubbleBody && bubbleBody.bubbleItem && bubbleBody.bubbleItem.roomBubblePalette && bubbleBody.bubbleItem.roomBubblePalette.link !== undefined)
                          ? bubbleBody.bubbleItem.roomBubblePalette.link
                          : palette.link
    mainMessageSurfaceColor: (bubbleBody && bubbleBody.bubbleItem && bubbleBody.bubbleItem.roomBubblePalette && bubbleBody.bubbleItem.roomBubblePalette.alternateBase !== undefined)
                             ? bubbleBody.bubbleItem.roomBubblePalette.alternateBase
                             : palette.alternateBase
    replyMessageTextColor: (bubbleBody && bubbleBody.replyItem && bubbleBody.replyItem.replyBubblePalette && bubbleBody.replyItem.replyBubblePalette.text !== undefined)
                           ? bubbleBody.replyItem.replyBubblePalette.text
                           : palette.text
    replyMessageSecondaryTextColor: (bubbleBody && bubbleBody.replyItem && bubbleBody.replyItem.replyBubblePalette && bubbleBody.replyItem.replyBubblePalette.buttonText !== undefined)
                                    ? bubbleBody.replyItem.replyBubblePalette.buttonText
                                    : palette.buttonText
    replyMessageLinkColor: (bubbleBody && bubbleBody.replyItem && bubbleBody.replyItem.replyBubblePalette && bubbleBody.replyItem.replyBubblePalette.link !== undefined)
                           ? bubbleBody.replyItem.replyBubblePalette.link
                           : palette.link
    replyMessageSurfaceColor: (bubbleBody && bubbleBody.replyItem && bubbleBody.replyItem.replyBubblePalette && bubbleBody.replyItem.replyBubblePalette.alternateBase !== undefined)
                              ? bubbleBody.replyItem.replyBubblePalette.alternateBase
                              : palette.alternateBase

    mainInset: threadId ? (4 + Komai.paddingSmall) : 0
    replyInset: mainInset + 4 + Komai.paddingMedium + Komai.paddingMedium

    property int bubbleMargin: Math.max(bubbleBody.metadataItem.width + Komai.paddingSmall + (wrapper.isStateEvent ? 0 : 2 * messageBubbleHorizontalPadding), Math.round((chat.delegateMaxWidth - avatarMargin) * (1 - Settings.timelineMessagesLayoutMaxWidthPercent / 100)))

    maxWidth: chat.delegateMaxWidth - avatarMargin - bubbleMargin
    hoverDismissTimerRef: bubbleBody.hoverDismissTimer

    function perfTimelineTypeLabel() {
        switch (wrapper.type) {
        case MtxEvent.TextMessage:
            return "text";
        case MtxEvent.NoticeMessage:
            return "notice";
        case MtxEvent.EmoteMessage:
            return "emote";
        case MtxEvent.ImageMessage:
            return "image";
        case MtxEvent.VideoMessage:
            return "video";
        case MtxEvent.AudioMessage:
            return "audio";
        case MtxEvent.FileMessage:
            return "file";
        case MtxEvent.Sticker:
            return "sticker";
        case MtxEvent.Encrypted:
            return "encrypted";
        case MtxEvent.Redacted:
            return "redacted";
        case MtxEvent.Encryption:
            return "encryption";
        case MtxEvent.Member:
            return "member";
        default:
            return wrapper.isStateEvent ? "state" : "other";
        }
    }

    onContentReadyChanged: {
        if (!contentReady || perfLoggedContentReady || !TimelineManager.roomSwitchPerfEnabled())
            return;

        perfLoggedContentReady = true;
        const roomId = wrapper.room ? String(wrapper.room.roomId || "") : String(wrapper.roomIdForColorCoding || "");
        if (roomId.length === 0)
            return;

        const typeLabel = perfTimelineTypeLabel();
        TimelineManager.markRoomSwitchPhase(roomId, "qml.matrix_row_ready." + typeLabel + "." + index);
        console.info("[room-switch-perf] phase=qml.matrix_row_ready"
            + " room='" + roomId + "'"
            + " type=" + typeLabel
            + " index=" + index
            + " height=" + Math.round(height)
            + " bubbleHeight=" + Math.round(bubbleBody.implicitHeight)
            + " event='" + String(wrapper.eventId || "") + "'");
    }

    data: [
        Loader {
            id: section

            active: !wrapper.perfDisableTimelineSectionHeaders && wrapper.startsNewMessageGroup
            //asynchronous: true
            sourceComponent: TimelineBubbleSectionHeader {
                day: wrapper.day
                isSender: wrapper.isSender
                isStateEvent: wrapper.isStateEvent
                parentWidth: wrapper.width
                roomRef: wrapper.roomForColorCoding
                colorRoomId: wrapper.roomIdForColorCoding
                previousMessageDay: wrapper.previousMessageDay
                previousMessageTimestamp: wrapper.previousMessageTimestamp
                previousMessageIsStateEvent: wrapper.previousMessageIsStateEvent
                previousMessageUserId: wrapper.previousMessageUserId
                timestamp: wrapper.timestamp
                userId: wrapper.userId
                userName: wrapper.userName
                userPowerlevel: wrapper.userPowerlevel
            }
            visible: status == Loader.Ready
            z: 4
        },
        AvatarUserFlipButton {
            id: messageUserAvatar

            property int avatarSide: Math.round(Komai.iconSize * (Settings.timelineMessagesLayoutAvatarSize === Settings.AvatarSize.Small ? 0.5 : 1))

            avatarButtonSize: avatarSide
            cleanFront: true
            avatarDisplayName: wrapper.userName
            avatarUrl: wrapper.avatarImageUrl(wrapper.userId)
            avatarUserId: wrapper.userId
            avatarRoomId: wrapper.roomIdForColorCoding
            toolTipText: wrapper.userId
            width: avatarSide
            height: avatarSide

            visible: wrapper.shouldShowMessageAvatar && wrapper.startsNewMessageGroup

            x: wrapper.avatarIsOnRight ? (wrapper.width - width) : 0
            y: (section.visible && section.active ? section.y + section.height : 0)
            z: 5

            onLeftClicked: {
                if (wrapper.roomIdForColorCoding && wrapper.userId)
                    TimelineManager.openRoomUserProfile(wrapper.roomIdForColorCoding, wrapper.userId)
            }

            Connections {
                function onRoomAvatarUrlChanged() {
                    messageUserAvatar.avatarUrl = wrapper.avatarImageUrl(wrapper.userId);
                }
                target: wrapper.room && typeof wrapper.room.roomAvatarUrlChanged === "function"
                    ? wrapper.room : null
            }
        },
        TimelineBubbleBody {
            id: bubbleBody
            wrapper: wrapper
            topOffset: section.visible && section.active ? section.y + section.height : 0
        },
        Loader {
            id: reactionRowLoader

            active: !wrapper.perfDisableTimelineReactions
                && wrapper.reactions
                && wrapper.reactions.length > 0
            anchors.top: bubbleBody.bottom
            anchors.topMargin: 1
            width: bubbleBody.width
            x: bubbleBody.x

            sourceComponent: Reactions {
                eventId: wrapper.eventId
                layoutDirection: (!wrapper.isStateEvent && wrapper.messageIsRightAligned) ? Qt.RightToLeft : Qt.LeftToRight
                reactions: wrapper.reactions
                width: bubbleBody.width
            }
        },
        Loader {
            id: unreadRowLoader

            active: wrapper.hasRoom
                && wrapper.index > 0
                && wrapper.room.fullyReadEventId == wrapper.eventId
            anchors {
                left: parent.left
                right: parent.right
                top: reactionRowLoader.bottom
                topMargin: 5
            }

            sourceComponent: Component {
                Item {
                    height: 3 + Komai.paddingSmall

                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.top: parent.top
                        color: palette.highlight
                        height: 3
                    }
                }
            }
        }
    ]
}
