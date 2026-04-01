// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include <optional>

#include <mtx/events.hpp>
#include <mtx/events/common.hpp>
#include <mtx/events/event_type.hpp>

namespace {

std::optional<std::string>
firstRelationEventId(mtx::common::RelationType type,
                     const mtx::common::Relations &relations,
                     bool includeFallback)
{
    for (const auto &relation : relations.relations) {
        if (relation.rel_type == type && (includeFallback || !relation.is_fallback))
            return relation.event_id;
    }

    return std::nullopt;
}

} // namespace

namespace mtx::common {

std::optional<std::string>
Relations::reply_to(bool include_fallback) const
{
    return firstRelationEventId(RelationType::InReplyTo, *this, include_fallback);
}

std::optional<std::string>
Relations::replaces(bool include_fallback) const
{
    return firstRelationEventId(RelationType::Replace, *this, include_fallback);
}

std::optional<std::string>
Relations::references(bool include_fallback) const
{
    return firstRelationEventId(RelationType::Reference, *this, include_fallback);
}

std::optional<std::string>
Relations::thread(bool include_fallback) const
{
    return firstRelationEventId(RelationType::Thread, *this, include_fallback);
}

std::optional<Relation>
Relations::annotates(bool include_fallback) const
{
    for (const auto &relation : relations) {
        if (relation.rel_type == RelationType::Annotation &&
            (include_fallback || !relation.is_fallback)) {
            return relation;
        }
    }

    return std::nullopt;
}

} // namespace mtx::common

namespace mtx::events {

std::string
to_string(EventType type)
{
    switch (type) {
    case EventType::KeyVerificationCancel:
        return "m.key.verification.cancel";
    case EventType::KeyVerificationRequest:
        return "m.key.verification.request";
    case EventType::KeyVerificationStart:
        return "m.key.verification.start";
    case EventType::KeyVerificationAccept:
        return "m.key.verification.accept";
    case EventType::KeyVerificationKey:
        return "m.key.verification.key";
    case EventType::KeyVerificationReady:
        return "m.key.verification.ready";
    case EventType::KeyVerificationDone:
        return "m.key.verification.done";
    case EventType::KeyVerificationMac:
        return "m.key.verification.mac";
    case EventType::Reaction:
        return "m.reaction";
    case EventType::Presence:
        return "m.presence";
    case EventType::PushRules:
        return "m.push_rules";
    case EventType::RoomKey:
        return "m.room_key";
    case EventType::ForwardedRoomKey:
        return "m.forwarded_room_key";
    case EventType::RoomKeyRequest:
        return "m.room_key_request";
    case EventType::RoomAliases:
        return "m.room.aliases";
    case EventType::RoomAvatar:
        return "m.room.avatar";
    case EventType::RoomCanonicalAlias:
        return "m.room.canonical_alias";
    case EventType::RoomCreate:
        return "m.room.create";
    case EventType::RoomEncrypted:
        return "m.room.encrypted";
    case EventType::Dummy:
        return "m.dummy";
    case EventType::RoomEncryption:
        return "m.room.encryption";
    case EventType::RoomGuestAccess:
        return "m.room.guest_access";
    case EventType::RoomHistoryVisibility:
        return "m.room.history_visibility";
    case EventType::RoomJoinRules:
        return "m.room.join_rules";
    case EventType::RoomMember:
        return "m.room.member";
    case EventType::RoomMessage:
        return "m.room.message";
    case EventType::RoomName:
        return "m.room.name";
    case EventType::RoomPowerLevels:
        return "m.room.power_levels";
    case EventType::RoomTopic:
        return "m.room.topic";
    case EventType::Widget:
        return "m.widget";
    case EventType::VectorWidget:
        return "im.vector.modular.widgets";
    case EventType::RoomRedaction:
        return "m.room.redaction";
    case EventType::RoomPinnedEvents:
        return "m.room.pinned_events";
    case EventType::RoomTombstone:
        return "m.room.tombstone";
    case EventType::RoomServerAcl:
        return "m.room.server_acl";
    case EventType::Sticker:
        return "m.sticker";
    case EventType::PolicyRuleUser:
        return "m.policy.rule.user";
    case EventType::PolicyRuleRoom:
        return "m.policy.rule.room";
    case EventType::PolicyRuleServer:
        return "m.policy.rule.server";
    case EventType::SpaceChild:
        return "m.space.child";
    case EventType::SpaceParent:
        return "m.space.parent";
    case EventType::Tag:
        return "m.tag";
    case EventType::Direct:
        return "m.direct";
    case EventType::CallInvite:
        return "m.call.invite";
    case EventType::CallCandidates:
        return "m.call.candidates";
    case EventType::CallAnswer:
        return "m.call.answer";
    case EventType::CallHangUp:
        return "m.call.hangup";
    case EventType::CallSelectAnswer:
        return "m.call.select_answer";
    case EventType::CallReject:
        return "m.call.reject";
    case EventType::CallNegotiate:
        return "m.call.negotiate";
    case EventType::Receipt:
        return "m.receipt";
    case EventType::Typing:
        return "m.typing";
    case EventType::FullyRead:
        return "m.fully_read";
    case EventType::IgnoredUsers:
        return "m.ignored_user_list";
    case EventType::NhekoHiddenEvents:
        return "im.nheko.hidden_events";
    case EventType::NhekoEventExpiry:
        return "im.nheko.event_expiry";
    case EventType::NhekoInvitePermissions:
        return "im.nheko.invite_permissions";
    case EventType::ImagePackInRoom:
        return "im.ponies.room_emotes";
    case EventType::ImagePackInAccountData:
        return "im.ponies.user_emotes";
    case EventType::ImagePackRooms:
        return "im.ponies.emote_rooms";
    case EventType::SecretSend:
        return "m.secret.send";
    case EventType::SecretRequest:
        return "m.secret.request";
    case EventType::Unsupported:
        return "";
    }

    return "";
}

MessageType
getMessageType(const std::string &type)
{
    if (type == "m.audio")
        return MessageType::Audio;
    if (type == "m.emote")
        return MessageType::Emote;
    if (type == "m.file")
        return MessageType::File;
    if (type == "m.image")
        return MessageType::Image;
    if (type == "m.location")
        return MessageType::Location;
    if (type == "m.notice")
        return MessageType::Notice;
    if (type == "m.text")
        return MessageType::Text;
    if (type == "nic.custom.confetti" || type == "nic.custom.fireworks" ||
        type == "io.element.effect.rainfall" || type == "io.element.effect.hearts" ||
        type == "io.element.effect.snowfall" || type == "io.element.effects.space_invaders")
        return MessageType::ElementEffect;
    if (type == "m.video")
        return MessageType::Video;
    if (type == "m.key.verification.request")
        return MessageType::KeyVerificationRequest;

    return MessageType::Unknown;
}

} // namespace mtx::events
