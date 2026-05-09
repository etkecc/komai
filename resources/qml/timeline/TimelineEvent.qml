// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../components" as Components
import QtQuick
import QtQuick.Layouts
import QtQuick.Window
import cc.etke.komai 1.0

EventDelegateChooser {
    id: wrapper

    required property bool isStateEvent
    property var previewData: ({})
    property var replyPreviewData: ({})
    property var roomAdapter: null
    property alias roomModelOverride: wrapper.roomAdapter
    readonly property var effectiveRoomContext: roomAdapter ? roomAdapter : room
    property string formattedBody: ""
    property string formattedStateEvent: ""
    property string stateEventIconSource: ""
    property string stateEventIconColorCategory: ""
    readonly property int colorRevision: TimelineManager.colorRevision
    property bool scrolledToThis: false
    property QtObject styleProfile: TimelineStyleProfile {}
    property QtObject resolvedStyleProfile: styleProfile
    property color mainMessageTextColor: palette.text
    property color mainMessageSecondaryTextColor: palette.buttonText
    property color mainMessageLinkColor: palette.link
    property color mainMessageSurfaceColor: palette.alternateBase
    property color replyMessageTextColor: palette.text
    property color replyMessageSecondaryTextColor: palette.buttonText
    property color replyMessageLinkColor: palette.link
    property color replyMessageSurfaceColor: palette.alternateBase

    onPreviewDataChanged: {
        if (!room)
            refreshDelegates();
    }

    onReplyPreviewDataChanged: {
        if (!room)
            refreshDelegates();
    }

    data: [
        Connections {
            target: wrapper
            ignoreUnknownSignals: true

            function onTypeChanged() {
                if (!wrapper.room)
                    wrapper.refreshDelegates();
            }
        }
    ]

    // qmllint disable required
    EventDelegateChoice {
        roleValues: [MtxEvent.TextMessage, MtxEvent.NoticeMessage, MtxEvent.ElementEffectMessage, MtxEvent.UnknownMessage,]

        TextMessage {
            required property string formattedBody
            required property int type
            required property string userId
            required property string userName

            Layout.fillWidth: true
            //Layout.maximumWidth: implicitWidth
            readonly property color chooserMainTextColor: (parent && parent.mainMessageTextColor !== undefined && parent.mainMessageTextColor !== null) ? parent.mainMessageTextColor : palette.text
            readonly property color chooserMainSecondaryTextColor: (parent && parent.mainMessageSecondaryTextColor !== undefined && parent.mainMessageSecondaryTextColor !== null) ? parent.mainMessageSecondaryTextColor : palette.buttonText
            readonly property color chooserMainLinkColor: (parent && parent.mainMessageLinkColor !== undefined && parent.mainMessageLinkColor !== null) ? parent.mainMessageLinkColor : palette.link
            readonly property color chooserMainSurfaceColor: (parent && parent.mainMessageSurfaceColor !== undefined && parent.mainMessageSurfaceColor !== null) ? parent.mainMessageSurfaceColor : palette.alternateBase
            readonly property color chooserReplyTextColor: (parent && parent.replyMessageTextColor !== undefined && parent.replyMessageTextColor !== null) ? parent.replyMessageTextColor : palette.text
            readonly property color chooserReplySecondaryTextColor: (parent && parent.replyMessageSecondaryTextColor !== undefined && parent.replyMessageSecondaryTextColor !== null) ? parent.replyMessageSecondaryTextColor : palette.buttonText
            readonly property color chooserReplyLinkColor: (parent && parent.replyMessageLinkColor !== undefined && parent.replyMessageLinkColor !== null) ? parent.replyMessageLinkColor : palette.link
            readonly property color chooserReplySurfaceColor: (parent && parent.replyMessageSurfaceColor !== undefined && parent.replyMessageSurfaceColor !== null) ? parent.replyMessageSurfaceColor : palette.alternateBase

            color: type == MtxEvent.NoticeMessage
                   ? (EventDelegateChooser.isReply
                      ? chooserReplySecondaryTextColor
                      : chooserMainSecondaryTextColor)
                   : (EventDelegateChooser.isReply
                      ? chooserReplyTextColor
                      : chooserMainTextColor)
            linkColor: EventDelegateChooser.isReply
                       ? chooserReplyLinkColor
                       : chooserMainLinkColor
            surfaceColor: EventDelegateChooser.isReply
                          ? chooserReplySurfaceColor
                          : chooserMainSurfaceColor
            eventType: type
            font.italic: type == MtxEvent.NoticeMessage
            formatted: formattedBody
            keepFullText: true
        }
    }
    EventDelegateChoice {
        roleValues: [MtxEvent.EmoteMessage,]

        TextMessage {
            required property string formattedBody
            required property string userId
            required property string userName

            Layout.fillWidth: true
            //Layout.maximumWidth: implicitWidth
            readonly property int chooserColorRevision: (parent && parent.colorRevision !== undefined && parent.colorRevision !== null) ? parent.colorRevision : 0
            readonly property string chooserRoomIdForColorCoding: (parent && parent.roomIdForColorCoding !== undefined && parent.roomIdForColorCoding !== null)
                ? String(parent.roomIdForColorCoding)
                : ""
            readonly property var chooserRoomForColorCoding: (parent && parent.roomForColorCoding !== undefined)
                ? parent.roomForColorCoding
                : null
            readonly property color chooserMainLinkColor: (parent && parent.mainMessageLinkColor !== undefined && parent.mainMessageLinkColor !== null) ? parent.mainMessageLinkColor : palette.link
            readonly property color chooserMainSurfaceColor: (parent && parent.mainMessageSurfaceColor !== undefined && parent.mainMessageSurfaceColor !== null) ? parent.mainMessageSurfaceColor : palette.alternateBase
            readonly property color chooserReplyLinkColor: (parent && parent.replyMessageLinkColor !== undefined && parent.replyMessageLinkColor !== null) ? parent.replyMessageLinkColor : palette.link
            readonly property color chooserReplySurfaceColor: (parent && parent.replyMessageSurfaceColor !== undefined && parent.replyMessageSurfaceColor !== null) ? parent.replyMessageSurfaceColor : palette.alternateBase

            color: Komai.readableAccentTextColor(
                (function() {
                    const _revision = chooserColorRevision;
                    if (chooserRoomIdForColorCoding.length > 0) {
                        if (chooserRoomIdForColorCoding.startsWith("!timeline-preview:")
                                && chooserRoomForColorCoding
                                && chooserRoomForColorCoding.roomMemberCount !== undefined) {
                            return TimelineManager.previewRoomUserColor(
                                        chooserRoomIdForColorCoding,
                                        userId,
                                        palette.base,
                                        Number(chooserRoomForColorCoding.roomMemberCount),
                                        Settings.timelineUserColorCodingPolicy);
                        }
                        return TimelineManager.roomUserColor(
                                    chooserRoomIdForColorCoding,
                                    userId,
                                    palette.base,
                                    Settings.timelineUserColorCodingPolicy);
                    }

                    return TimelineManager.userColor(userId, palette.base);
                })(),
                palette.base)
            linkColor: EventDelegateChooser.isReply
                       ? chooserReplyLinkColor
                       : chooserMainLinkColor
            surfaceColor: EventDelegateChooser.isReply
                          ? chooserReplySurfaceColor
                          : chooserMainSurfaceColor
            formatted: {
                var prefix = TimelineManager.escapeEmoji(userName) + " ";
                var body = formattedBody;
                // Strip the outer <p> wrapper so the prefix sits on the same
                // line as the body, and convert paragraph breaks into line
                // breaks so multi-paragraph emotes still break visually.
                var inline = body
                    .replace(/<\/p>\s*<p[^>]*>/gi, "<br><br>")
                    .replace(/^\s*<p[^>]*>/i, "")
                    .replace(/<\/p>\s*$/i, "");
                return prefix + inline;
            }
            isOnlyEmoji: 0
            keepFullText: true
        }
    }
    EventDelegateChoice {
        roleValues: [MtxEvent.CanonicalAlias, MtxEvent.ServerAcl, MtxEvent.Name, MtxEvent.Topic, MtxEvent.Avatar, MtxEvent.PinnedEvents, MtxEvent.ImagePackInRoom, MtxEvent.SpaceParent, MtxEvent.SpaceChild, MtxEvent.RoomCreate, MtxEvent.PowerLevels, MtxEvent.PolicyRuleUser, MtxEvent.PolicyRuleRoom, MtxEvent.PolicyRuleServer, MtxEvent.RoomJoinRules, MtxEvent.RoomHistoryVisibility, MtxEvent.RoomGuestAccess,]
        StateEventMessage {
            required property string formattedStateEvent
            required property string userId
            required property string userName

            Layout.fillWidth: true
            body: formatted
            formatted: formattedStateEvent
            isOnlyEmoji: 0
            isReply: EventDelegateChooser.isReply
            isStateEvent: true
            readonly property color chooserMainSecondaryTextColor: (parent && parent.mainMessageSecondaryTextColor !== undefined && parent.mainMessageSecondaryTextColor !== null) ? parent.mainMessageSecondaryTextColor : palette.buttonText
            readonly property color chooserMainLinkColor: (parent && parent.mainMessageLinkColor !== undefined && parent.mainMessageLinkColor !== null) ? parent.mainMessageLinkColor : palette.link
            readonly property color chooserMainSurfaceColor: (parent && parent.mainMessageSurfaceColor !== undefined && parent.mainMessageSurfaceColor !== null) ? parent.mainMessageSurfaceColor : palette.alternateBase
            readonly property color chooserReplySecondaryTextColor: (parent && parent.replyMessageSecondaryTextColor !== undefined && parent.replyMessageSecondaryTextColor !== null) ? parent.replyMessageSecondaryTextColor : palette.buttonText
            readonly property color chooserReplyLinkColor: (parent && parent.replyMessageLinkColor !== undefined && parent.replyMessageLinkColor !== null) ? parent.replyMessageLinkColor : palette.link
            readonly property color chooserReplySurfaceColor: (parent && parent.replyMessageSurfaceColor !== undefined && parent.replyMessageSurfaceColor !== null) ? parent.replyMessageSurfaceColor : palette.alternateBase
            color: EventDelegateChooser.isReply
                   ? chooserReplySecondaryTextColor
                   : chooserMainSecondaryTextColor
            linkColor: EventDelegateChooser.isReply
                       ? chooserReplyLinkColor
                       : chooserMainLinkColor
            surfaceColor: EventDelegateChooser.isReply
                          ? chooserReplySurfaceColor
                          : chooserMainSurfaceColor
            keepFullText: true
        }
    }
    EventDelegateChoice {
        roleValues: [MtxEvent.CallInvite,]

        TextMessage {
            required property string callType
            required property string userId
            required property string userName

            Layout.fillWidth: true
            body: formatted
            readonly property color chooserMainSecondaryTextColor: (parent && parent.mainMessageSecondaryTextColor !== undefined && parent.mainMessageSecondaryTextColor !== null) ? parent.mainMessageSecondaryTextColor : palette.buttonText
            readonly property color chooserMainLinkColor: (parent && parent.mainMessageLinkColor !== undefined && parent.mainMessageLinkColor !== null) ? parent.mainMessageLinkColor : palette.link
            readonly property color chooserMainSurfaceColor: (parent && parent.mainMessageSurfaceColor !== undefined && parent.mainMessageSurfaceColor !== null) ? parent.mainMessageSurfaceColor : palette.alternateBase
            readonly property color chooserReplySecondaryTextColor: (parent && parent.replyMessageSecondaryTextColor !== undefined && parent.replyMessageSecondaryTextColor !== null) ? parent.replyMessageSecondaryTextColor : palette.buttonText
            readonly property color chooserReplyLinkColor: (parent && parent.replyMessageLinkColor !== undefined && parent.replyMessageLinkColor !== null) ? parent.replyMessageLinkColor : palette.link
            readonly property color chooserReplySurfaceColor: (parent && parent.replyMessageSurfaceColor !== undefined && parent.replyMessageSurfaceColor !== null) ? parent.replyMessageSurfaceColor : palette.alternateBase
            color: EventDelegateChooser.isReply
                   ? chooserReplySecondaryTextColor
                   : chooserMainSecondaryTextColor
            linkColor: EventDelegateChooser.isReply
                       ? chooserReplyLinkColor
                       : chooserMainLinkColor
            surfaceColor: EventDelegateChooser.isReply
                          ? chooserReplySurfaceColor
                          : chooserMainSurfaceColor
            font.italic: true
            formatted: {
                switch (callType) {
                case "voice":
                    return qsTr("%1 placed a voice call.").arg(TimelineManager.escapeEmoji(userName));
                case "video":
                    return qsTr("%1 placed a video call.").arg(TimelineManager.escapeEmoji(userName));
                default:
                    return qsTr("%1 placed a call.").arg(TimelineManager.escapeEmoji(userName));
                }
            }
            isOnlyEmoji: 0
            keepFullText: true
        }
    }
    EventDelegateChoice {
        roleValues: [MtxEvent.CallAnswer, MtxEvent.CallReject, MtxEvent.CallSelectAnswer, MtxEvent.CallHangUp, MtxEvent.CallCandidates, MtxEvent.CallNegotiate,]

        TextMessage {
            required property int type
            required property string userId
            required property string userName

            Layout.fillWidth: true
            body: formatted
            readonly property color chooserMainSecondaryTextColor: (parent && parent.mainMessageSecondaryTextColor !== undefined && parent.mainMessageSecondaryTextColor !== null) ? parent.mainMessageSecondaryTextColor : palette.buttonText
            readonly property color chooserMainLinkColor: (parent && parent.mainMessageLinkColor !== undefined && parent.mainMessageLinkColor !== null) ? parent.mainMessageLinkColor : palette.link
            readonly property color chooserMainSurfaceColor: (parent && parent.mainMessageSurfaceColor !== undefined && parent.mainMessageSurfaceColor !== null) ? parent.mainMessageSurfaceColor : palette.alternateBase
            readonly property color chooserReplySecondaryTextColor: (parent && parent.replyMessageSecondaryTextColor !== undefined && parent.replyMessageSecondaryTextColor !== null) ? parent.replyMessageSecondaryTextColor : palette.buttonText
            readonly property color chooserReplyLinkColor: (parent && parent.replyMessageLinkColor !== undefined && parent.replyMessageLinkColor !== null) ? parent.replyMessageLinkColor : palette.link
            readonly property color chooserReplySurfaceColor: (parent && parent.replyMessageSurfaceColor !== undefined && parent.replyMessageSurfaceColor !== null) ? parent.replyMessageSurfaceColor : palette.alternateBase
            color: EventDelegateChooser.isReply
                   ? chooserReplySecondaryTextColor
                   : chooserMainSecondaryTextColor
            linkColor: EventDelegateChooser.isReply
                       ? chooserReplyLinkColor
                       : chooserMainLinkColor
            surfaceColor: EventDelegateChooser.isReply
                          ? chooserReplySurfaceColor
                          : chooserMainSurfaceColor
            font.italic: true
            formatted: {
                switch (type) {
                case MtxEvent.CallAnswer:
                    return qsTr("%1 answered the call.").arg(TimelineManager.escapeEmoji(userName));
                case MtxEvent.CallReject:
                    return qsTr("%1 rejected the call.").arg(TimelineManager.escapeEmoji(userName));
                case MtxEvent.CallSelectAnswer:
                    return qsTr("%1 selected answer.").arg(TimelineManager.escapeEmoji(userName));
                case MtxEvent.CallHangUp:
                    return qsTr("%1 ended the call.").arg(TimelineManager.escapeEmoji(userName));
                case MtxEvent.CallCandidates:
                    return qsTr("%1 is negotiating the call...").arg(TimelineManager.escapeEmoji(userName));
                case MtxEvent.CallNegotiate:
                    return qsTr("%1 is negotiating the call...").arg(TimelineManager.escapeEmoji(userName));
                }
            }
            isOnlyEmoji: 0
            keepFullText: true
        }
    }
    EventDelegateChoice {
        roleValues: [MtxEvent.ImageMessage,]

        ImageMessage {
            required property string userId
            required property string userName

            Layout.fillWidth: true
            blurhash: ""
            //Layout.maximumWidth: tempWidth
            //Layout.maximumHeight: timelineView.height / 8
            containerHeight: timelineView ? timelineView.height : Screen.height
        }
    }
    EventDelegateChoice {
        roleValues: [MtxEvent.Sticker,]

        StickerMessage {
            required property string userId
            required property string userName

            Layout.fillWidth: true
            blurhash: ""
            containerHeight: timelineView ? timelineView.height : Screen.height
        }
    }
    EventDelegateChoice {
        roleValues: [MtxEvent.FileMessage,]

        FileMessage {
            required property string userId
            required property string userName
            property QtObject fallbackStyleProfile: TimelineStyleProfile {}
            readonly property QtObject chooserStyleProfile: (parent && parent.resolvedStyleProfile !== undefined && parent.resolvedStyleProfile !== null)
                ? parent.resolvedStyleProfile
                : fallbackStyleProfile

            Layout.fillWidth: true
            styleProfile: chooserStyleProfile
        }
    }
    EventDelegateChoice {
        roleValues: [MtxEvent.VideoMessage,]

        VideoMessage {
            required property string userId
            required property string userName

            Layout.fillWidth: true
        }
    }
    EventDelegateChoice {
        roleValues: [MtxEvent.AudioMessage,]

        AudioMessage {
            required property string userId
            required property string userName

            Layout.fillWidth: true
        }
    }
    EventDelegateChoice {
        roleValues: [MtxEvent.Encrypted,]

        Encrypted {
            required property string userId
            required property string userName
            property QtObject fallbackStyleProfile: TimelineStyleProfile {}
            readonly property QtObject chooserStyleProfile: (parent && parent.resolvedStyleProfile !== undefined && parent.resolvedStyleProfile !== null)
                ? parent.resolvedStyleProfile
                : fallbackStyleProfile

            Layout.fillWidth: true
            styleProfile: chooserStyleProfile
        }
    }
    EventDelegateChoice {
        roleValues: [MtxEvent.Encryption,]

        EncryptionEnabled {
            required property string userId

            Layout.fillWidth: true
        }
    }
    EventDelegateChoice {
        roleValues: [MtxEvent.Redacted]

        Redacted {
            required property string userId
            required property string userName

            Layout.fillWidth: true
        }
    }
    EventDelegateChoice {
        roleValues: [MtxEvent.Member]

        ColumnLayout {
            id: member

            required property string formattedStateEvent
            required property string stateEventIconSource
            required property string stateEventIconColorCategory
            required property var room
            required property string userId
            required property string userName
            property bool stateEventIconOnRight: false
            readonly property bool hasKnockAction: room && typeof room.showAcceptKnockButton === "function" && room.showAcceptKnockButton(eventId)

            StateEventMessage {
                Layout.fillWidth: true
                body: formatted
                formatted: member.formattedStateEvent
                isOnlyEmoji: 0
                isReply: EventDelegateChooser.isReply
                isStateEvent: true
                stateEventIconSource: member.stateEventIconSource
                stateEventIconColorCategory: member.stateEventIconColorCategory
                stateEventIconOnRight: member.stateEventIconOnRight
                keepFullText: true
            }
            Components.KomaiButton {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Allow them in")
                visible: member.hasKnockAction

                onClicked: {
                    if (member.room)
                        member.room.acceptKnock(member.eventId)
                }
            }
        }
    }
    EventDelegateChoice {
        roleValues: [MtxEvent.Tombstone]

        ColumnLayout {
            id: tombstone

            required property string body
            required property string eventId
            required property var room
            required property string userId
            required property string userName
            property bool stateEventIconOnRight: false

            StateEventMessage {
                Layout.fillWidth: true
                body: formatted
                formatted: qsTr("This room was replaced for the following reason: %1").arg(tombstone.body)
                isOnlyEmoji: 0
                isReply: EventDelegateChooser.isReply
                isStateEvent: true
                stateEventIconColorCategory: "negative"
                stateEventIconOnRight: tombstone.stateEventIconOnRight
                keepFullText: true
            }
            Components.KomaiButton {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("Go to replacement room")

                onClicked: tombstone.room.joinReplacementRoom(tombstone.eventId)
            }
        }
    }
    EventDelegateChoice {
        roleValues: []

        MatrixText {
            property string typeString: ""
            property string matrixEventType: ""
            property bool isStateEvent: false
            required property string userId
            required property string userName

            Layout.fillWidth: true
            text: {
                const label = matrixEventType !== "" ? matrixEventType : typeString;
                if (label === "")
                    return qsTr("Unsupported message");
                return isStateEvent
                    ? qsTr("Unsupported state event (%1)").arg(label)
                    : qsTr("Unsupported event (%1)").arg(label);
            }
        }
    }
    // qmllint enable required
}
