// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TimelineEventTypes.h"

std::vector<qml_mtx_events::EventType>
qml_mtx_events::defaultHiddenEventTypes()
{
    return {
      qml_mtx_events::Reaction,
      qml_mtx_events::CallCandidates,
      qml_mtx_events::CallNegotiate,
      qml_mtx_events::CallSelectAnswer,
    };
}

QStringList
qml_mtx_events::defaultHiddenTimelineEventTypeKeys()
{
    QStringList result;

    for (const auto eventType : defaultHiddenEventTypes()) {
        const auto key = localTimelineEventTypeKey(eventType);
        if (!key.isEmpty())
            result.push_back(key);
    }

    return result;
}

QString
qml_mtx_events::localTimelineEventTypeKey(EventType type)
{
    switch (type) {
    case Reaction:
        return QStringLiteral("m.reaction");
    case CallCandidates:
        return QStringLiteral("m.call.candidates");
    case CallSelectAnswer:
        return QStringLiteral("m.call.select_answer");
    case CallNegotiate:
        return QStringLiteral("m.call.negotiate");
    case Member:
        return QStringLiteral("m.room.member");
    case PowerLevels:
        return QStringLiteral("m.room.power_levels");
    case ServerAcl:
        return QStringLiteral("m.room.server_acl");
    case Sticker:
        return QStringLiteral("m.sticker");
    case Unsupported:
        return QStringLiteral("unsupported");
    default:
        return QString();
    }
}

std::optional<qml_mtx_events::EventType>
qml_mtx_events::localTimelineEventTypeFromKey(const QString &key)
{
    const auto normalized = key.trimmed().toLower();
    if (normalized == QStringLiteral("m.reaction"))
        return Reaction;
    if (normalized == QStringLiteral("m.call.candidates"))
        return CallCandidates;
    if (normalized == QStringLiteral("m.call.select_answer"))
        return CallSelectAnswer;
    if (normalized == QStringLiteral("m.call.negotiate"))
        return CallNegotiate;
    if (normalized == QStringLiteral("m.room.member"))
        return Member;
    if (normalized == QStringLiteral("m.room.power_levels"))
        return PowerLevels;
    if (normalized == QStringLiteral("m.room.server_acl"))
        return ServerAcl;
    if (normalized == QStringLiteral("m.sticker"))
        return Sticker;
    if (normalized == QStringLiteral("unsupported"))
        return Unsupported;

    return std::nullopt;
}

qml_mtx_events::EventType
qml_mtx_events::matrixTimelineEventType(const QString &itemKind, const QString &matrixEventType)
{
    const auto normalizedKind      = itemKind.trimmed().toLower();
    const auto normalizedEventType = matrixEventType.trimmed().toLower();

    if (normalizedKind == QStringLiteral("redacted"))
        return Redacted;
    if (normalizedKind == QStringLiteral("unable_to_decrypt"))
        return Encrypted;
    if (normalizedKind == QStringLiteral("message"))
        return normalizedEventType.isEmpty() ||
                   normalizedEventType == QStringLiteral("m.room.message")
                 ? TextMessage
                 : UnknownMessage;
    if (normalizedKind == QStringLiteral("notice"))
        return normalizedEventType.isEmpty() ||
                   normalizedEventType == QStringLiteral("m.room.message")
                 ? NoticeMessage
                 : UnknownMessage;
    if (normalizedKind == QStringLiteral("image"))
        return normalizedEventType.isEmpty() ||
                   normalizedEventType == QStringLiteral("m.room.message")
                 ? ImageMessage
                 : UnknownMessage;
    if (normalizedKind == QStringLiteral("video"))
        return normalizedEventType.isEmpty() ||
                   normalizedEventType == QStringLiteral("m.room.message")
                 ? VideoMessage
                 : UnknownMessage;
    if (normalizedKind == QStringLiteral("audio"))
        return normalizedEventType.isEmpty() ||
                   normalizedEventType == QStringLiteral("m.room.message")
                 ? AudioMessage
                 : UnknownMessage;
    if (normalizedKind == QStringLiteral("file"))
        return normalizedEventType.isEmpty() ||
                   normalizedEventType == QStringLiteral("m.room.message")
                 ? FileMessage
                 : UnknownMessage;
    if (normalizedKind == QStringLiteral("location"))
        return normalizedEventType.isEmpty() ||
                   normalizedEventType == QStringLiteral("m.room.message")
                 ? LocationMessage
                 : UnknownMessage;
    if (normalizedKind == QStringLiteral("sticker") ||
        normalizedEventType == QStringLiteral("m.sticker")) {
        return Sticker;
    }
    if (normalizedKind == QStringLiteral("emote"))
        return normalizedEventType.isEmpty() ||
                   normalizedEventType == QStringLiteral("m.room.message")
                 ? EmoteMessage
                 : UnknownMessage;
    if (normalizedKind == QStringLiteral("unknown_message"))
        return UnknownMessage;
    if (normalizedKind == QStringLiteral("membership_change") ||
        normalizedKind == QStringLiteral("profile_change") ||
        normalizedEventType == QStringLiteral("m.room.member")) {
        return Member;
    }
    if (normalizedEventType == QStringLiteral("m.room.power_levels"))
        return PowerLevels;
    if (normalizedEventType == QStringLiteral("m.room.server_acl"))
        return ServerAcl;
    if (normalizedEventType == QStringLiteral("m.room.name"))
        return Name;
    if (normalizedEventType == QStringLiteral("m.room.topic"))
        return Topic;
    if (normalizedEventType == QStringLiteral("m.room.avatar"))
        return Avatar;
    if (normalizedEventType == QStringLiteral("m.room.encryption"))
        return Encryption;
    if (normalizedEventType == QStringLiteral("m.room.pinned_events"))
        return PinnedEvents;
    if (normalizedEventType == QStringLiteral("m.room.join_rules"))
        return RoomJoinRules;
    if (normalizedEventType == QStringLiteral("m.room.history_visibility"))
        return RoomHistoryVisibility;
    if (normalizedEventType == QStringLiteral("m.room.guest_access"))
        return RoomGuestAccess;
    if (normalizedEventType == QStringLiteral("m.room.canonical_alias"))
        return CanonicalAlias;
    if (normalizedEventType == QStringLiteral("m.room.create"))
        return RoomCreate;
    if (normalizedEventType == QStringLiteral("m.room.tombstone"))
        return Tombstone;
    if (normalizedEventType == QStringLiteral("m.policy.rule.user"))
        return PolicyRuleUser;
    if (normalizedEventType == QStringLiteral("m.policy.rule.room"))
        return PolicyRuleRoom;
    if (normalizedEventType == QStringLiteral("m.policy.rule.server"))
        return PolicyRuleServer;
    if (normalizedEventType == QStringLiteral("m.space.parent"))
        return SpaceParent;
    if (normalizedEventType == QStringLiteral("m.space.child"))
        return SpaceChild;
    if (normalizedKind == QStringLiteral("call_invite") ||
        normalizedEventType == QStringLiteral("m.call.invite")) {
        return CallInvite;
    }
    if (normalizedEventType == QStringLiteral("m.call.answer"))
        return CallAnswer;
    if (normalizedEventType == QStringLiteral("m.call.hangup"))
        return CallHangUp;
    if (normalizedEventType == QStringLiteral("m.call.candidates"))
        return CallCandidates;
    if (normalizedEventType == QStringLiteral("m.call.select_answer"))
        return CallSelectAnswer;
    if (normalizedEventType == QStringLiteral("m.call.reject"))
        return CallReject;
    if (normalizedEventType == QStringLiteral("m.call.negotiate"))
        return CallNegotiate;
    if (normalizedEventType == QStringLiteral("m.reaction"))
        return Reaction;
    if (normalizedEventType == QStringLiteral("m.room.redaction"))
        return Redaction;
    if (normalizedEventType == QStringLiteral("m.room.message"))
        return UnknownMessage;
    if (normalizedKind == QStringLiteral("rtc_notification"))
        return CallNotification;
    if (normalizedKind == QStringLiteral("other_state") ||
        normalizedKind == QStringLiteral("failed_to_parse_state") ||
        normalizedKind == QStringLiteral("other_message") ||
        normalizedKind == QStringLiteral("failed_to_parse_message_like") ||
        normalizedKind == QStringLiteral("poll") ||
        normalizedKind == QStringLiteral("date_divider")) {
        return UnknownEvent;
    }

    return UnknownEvent;
}
