// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TimelineEventTypes.h"

#include "EventAccessors.h"

namespace {
struct RoomEventType
{
    template<class T>
    constexpr qml_mtx_events::EventType operator()(const mtx::events::Event<T> &e)
    {
        return qml_mtx_events::toRoomEventType(e.type);
    }
    constexpr qml_mtx_events::EventType
    operator()(const mtx::events::Event<mtx::events::msg::Audio> &)
    {
        return qml_mtx_events::EventType::AudioMessage;
    }
    constexpr qml_mtx_events::EventType
    operator()(const mtx::events::Event<mtx::events::msg::ElementEffect> &)
    {
        return qml_mtx_events::EventType::ElementEffectMessage;
    }
    constexpr qml_mtx_events::EventType
    operator()(const mtx::events::Event<mtx::events::msg::Emote> &)
    {
        return qml_mtx_events::EventType::EmoteMessage;
    }
    constexpr qml_mtx_events::EventType
    operator()(const mtx::events::Event<mtx::events::msg::File> &)
    {
        return qml_mtx_events::EventType::FileMessage;
    }
    constexpr qml_mtx_events::EventType
    operator()(const mtx::events::Event<mtx::events::msg::Image> &)
    {
        return qml_mtx_events::EventType::ImageMessage;
    }
    constexpr qml_mtx_events::EventType
    operator()(const mtx::events::Event<mtx::events::msg::Notice> &)
    {
        return qml_mtx_events::EventType::NoticeMessage;
    }
    constexpr qml_mtx_events::EventType
    operator()(const mtx::events::Event<mtx::events::msg::Text> &)
    {
        return qml_mtx_events::EventType::TextMessage;
    }
    constexpr qml_mtx_events::EventType
    operator()(const mtx::events::Event<mtx::events::msg::Unknown> &)
    {
        return qml_mtx_events::EventType::UnknownMessage;
    }
    constexpr qml_mtx_events::EventType
    operator()(const mtx::events::Event<mtx::events::msg::Video> &)
    {
        return qml_mtx_events::EventType::VideoMessage;
    }
    constexpr qml_mtx_events::EventType
    operator()(const mtx::events::Event<mtx::events::msg::KeyVerificationRequest> &)
    {
        return qml_mtx_events::EventType::KeyVerificationRequest;
    }
    constexpr qml_mtx_events::EventType
    operator()(const mtx::events::Event<mtx::events::msg::KeyVerificationStart> &)
    {
        return qml_mtx_events::EventType::KeyVerificationStart;
    }
    constexpr qml_mtx_events::EventType
    operator()(const mtx::events::Event<mtx::events::msg::KeyVerificationMac> &)
    {
        return qml_mtx_events::EventType::KeyVerificationMac;
    }
    constexpr qml_mtx_events::EventType
    operator()(const mtx::events::Event<mtx::events::msg::KeyVerificationAccept> &)
    {
        return qml_mtx_events::EventType::KeyVerificationAccept;
    }
    constexpr qml_mtx_events::EventType
    operator()(const mtx::events::Event<mtx::events::msg::KeyVerificationReady> &)
    {
        return qml_mtx_events::EventType::KeyVerificationReady;
    }
    constexpr qml_mtx_events::EventType
    operator()(const mtx::events::Event<mtx::events::msg::KeyVerificationCancel> &)
    {
        return qml_mtx_events::EventType::KeyVerificationCancel;
    }
    constexpr qml_mtx_events::EventType
    operator()(const mtx::events::Event<mtx::events::msg::KeyVerificationKey> &)
    {
        return qml_mtx_events::EventType::KeyVerificationKey;
    }
    constexpr qml_mtx_events::EventType
    operator()(const mtx::events::Event<mtx::events::msg::KeyVerificationDone> &)
    {
        return qml_mtx_events::EventType::KeyVerificationDone;
    }
    constexpr qml_mtx_events::EventType
    operator()(const mtx::events::Event<mtx::events::msg::Redacted> &)
    {
        return qml_mtx_events::EventType::Redacted;
    }
    constexpr qml_mtx_events::EventType
    operator()(const mtx::events::Event<mtx::events::voip::CallInvite> &)
    {
        return qml_mtx_events::EventType::CallInvite;
    }
    constexpr qml_mtx_events::EventType
    operator()(const mtx::events::Event<mtx::events::voip::CallAnswer> &)
    {
        return qml_mtx_events::EventType::CallAnswer;
    }
    constexpr qml_mtx_events::EventType
    operator()(const mtx::events::Event<mtx::events::voip::CallHangUp> &)
    {
        return qml_mtx_events::EventType::CallHangUp;
    }
    constexpr qml_mtx_events::EventType
    operator()(const mtx::events::Event<mtx::events::voip::CallCandidates> &)
    {
        return qml_mtx_events::EventType::CallCandidates;
    }
    constexpr qml_mtx_events::EventType
    operator()(const mtx::events::Event<mtx::events::voip::CallSelectAnswer> &)
    {
        return qml_mtx_events::EventType::CallSelectAnswer;
    }
    constexpr qml_mtx_events::EventType
    operator()(const mtx::events::Event<mtx::events::voip::CallReject> &)
    {
        return qml_mtx_events::EventType::CallReject;
    }
    constexpr qml_mtx_events::EventType
    operator()(const mtx::events::Event<mtx::events::voip::CallNegotiate> &)
    {
        return qml_mtx_events::EventType::CallNegotiate;
    }
};
}

qml_mtx_events::EventType
qml_mtx_events::toRoomEventType(mtx::events::EventType e)
{
    using mtx::events::EventType;
    switch (e) {
    case EventType::RoomKeyRequest:
        return qml_mtx_events::EventType::KeyRequest;
    case EventType::Reaction:
        return qml_mtx_events::EventType::Reaction;
    case EventType::RoomAliases:
        return qml_mtx_events::EventType::Aliases;
    case EventType::RoomAvatar:
        return qml_mtx_events::EventType::Avatar;
    case EventType::RoomCanonicalAlias:
        return qml_mtx_events::EventType::CanonicalAlias;
    case EventType::RoomCreate:
        return qml_mtx_events::EventType::RoomCreate;
    case EventType::RoomEncrypted:
        return qml_mtx_events::EventType::Encrypted;
    case EventType::RoomEncryption:
        return qml_mtx_events::EventType::Encryption;
    case EventType::RoomGuestAccess:
        return qml_mtx_events::EventType::RoomGuestAccess;
    case EventType::RoomHistoryVisibility:
        return qml_mtx_events::EventType::RoomHistoryVisibility;
    case EventType::RoomJoinRules:
        return qml_mtx_events::EventType::RoomJoinRules;
    case EventType::RoomMember:
        return qml_mtx_events::EventType::Member;
    case EventType::RoomMessage:
        return qml_mtx_events::EventType::UnknownEvent;
    case EventType::RoomName:
        return qml_mtx_events::EventType::Name;
    case EventType::RoomPowerLevels:
        return qml_mtx_events::EventType::PowerLevels;
    case EventType::RoomTopic:
        return qml_mtx_events::EventType::Topic;
    case EventType::RoomTombstone:
        return qml_mtx_events::EventType::Tombstone;
    case EventType::RoomServerAcl:
        return qml_mtx_events::EventType::ServerAcl;
    case EventType::RoomRedaction:
        return qml_mtx_events::EventType::Redaction;
    case EventType::RoomPinnedEvents:
        return qml_mtx_events::EventType::PinnedEvents;
    case EventType::Sticker:
        return qml_mtx_events::EventType::Sticker;
    case EventType::Tag:
        return qml_mtx_events::EventType::Tag;
    case EventType::PolicyRuleUser:
        return qml_mtx_events::EventType::PolicyRuleUser;
    case EventType::PolicyRuleRoom:
        return qml_mtx_events::EventType::PolicyRuleRoom;
    case EventType::PolicyRuleServer:
        return qml_mtx_events::EventType::PolicyRuleServer;
    case EventType::SpaceParent:
        return qml_mtx_events::EventType::SpaceParent;
    case EventType::SpaceChild:
        return qml_mtx_events::EventType::SpaceChild;
    case EventType::ImagePackInRoom:
        return qml_mtx_events::ImagePackInRoom;
    case EventType::ImagePackInAccountData:
        return qml_mtx_events::ImagePackInAccountData;
    case EventType::ImagePackRooms:
        return qml_mtx_events::ImagePackRooms;
    case EventType::Unsupported:
        return qml_mtx_events::EventType::Unsupported;
    default:
        return qml_mtx_events::EventType::UnknownEvent;
    }
}

qml_mtx_events::EventType
qml_mtx_events::toRoomEventType(const mtx::events::collections::TimelineEvents &event)
{
    return std::visit(RoomEventType{}, event);
}

QString
qml_mtx_events::toRoomEventTypeString(const mtx::events::collections::TimelineEvents &event)
{
    return QString::fromStdString(to_string(mtx::accessors::event_type(event)));
}

mtx::events::EventType
qml_mtx_events::fromRoomEventType(qml_mtx_events::EventType t)
{
    switch (t) {
    // Unsupported event
    case qml_mtx_events::Unsupported:
        return mtx::events::EventType::Unsupported;

    /// m.room_key_request
    case qml_mtx_events::KeyRequest:
        return mtx::events::EventType::RoomKeyRequest;
    /// m.reaction:
    case qml_mtx_events::Reaction:
        return mtx::events::EventType::Reaction;
    /// m.room.aliases
    case qml_mtx_events::Aliases:
        return mtx::events::EventType::RoomAliases;
    /// m.room.avatar
    case qml_mtx_events::Avatar:
        return mtx::events::EventType::RoomAvatar;
    /// m.call.invite
    case qml_mtx_events::CallInvite:
        return mtx::events::EventType::CallInvite;
    /// m.call.answer
    case qml_mtx_events::CallAnswer:
        return mtx::events::EventType::CallAnswer;
    /// m.call.hangup
    case qml_mtx_events::CallHangUp:
        return mtx::events::EventType::CallHangUp;
    /// m.call.candidates
    case qml_mtx_events::CallCandidates:
        return mtx::events::EventType::CallCandidates;
    /// m.call.select_answer
    case qml_mtx_events::CallSelectAnswer:
        return mtx::events::EventType::CallSelectAnswer;
    /// m.call.reject
    case qml_mtx_events::CallReject:
        return mtx::events::EventType::CallReject;
    /// m.call.negotiate
    case qml_mtx_events::CallNegotiate:
        return mtx::events::EventType::CallNegotiate;
    /// m.room.canonical_alias
    case qml_mtx_events::CanonicalAlias:
        return mtx::events::EventType::RoomCanonicalAlias;
    /// m.room.create
    case qml_mtx_events::RoomCreate:
        return mtx::events::EventType::RoomCreate;
    /// m.room.encrypted.
    case qml_mtx_events::Encrypted:
        return mtx::events::EventType::RoomEncrypted;
    /// m.room.encryption.
    case qml_mtx_events::Encryption:
        return mtx::events::EventType::RoomEncryption;
    /// m.room.guest_access
    case qml_mtx_events::RoomGuestAccess:
        return mtx::events::EventType::RoomGuestAccess;
    /// m.room.history_visibility
    case qml_mtx_events::RoomHistoryVisibility:
        return mtx::events::EventType::RoomHistoryVisibility;
    /// m.room.join_rules
    case qml_mtx_events::RoomJoinRules:
        return mtx::events::EventType::RoomJoinRules;
    /// m.room.member
    case qml_mtx_events::Member:
        return mtx::events::EventType::RoomMember;
    /// m.room.name
    case qml_mtx_events::Name:
        return mtx::events::EventType::RoomName;
    /// m.room.power_levels
    case qml_mtx_events::PowerLevels:
        return mtx::events::EventType::RoomPowerLevels;
    /// m.room.tombstone
    case qml_mtx_events::Tombstone:
        return mtx::events::EventType::RoomTombstone;
    /// m.room.server_acl
    case qml_mtx_events::ServerAcl:
        return mtx::events::EventType::RoomServerAcl;
    /// m.room.topic
    case qml_mtx_events::Topic:
        return mtx::events::EventType::RoomTopic;
    /// m.room.redaction
    case qml_mtx_events::Redaction:
        return mtx::events::EventType::RoomRedaction;
    /// m.room.pinned_events
    case qml_mtx_events::PinnedEvents:
        return mtx::events::EventType::RoomPinnedEvents;
    /// m.widget
    case qml_mtx_events::Widget:
        return mtx::events::EventType::Widget;
    // m.sticker
    case qml_mtx_events::Sticker:
        return mtx::events::EventType::Sticker;
    // m.tag
    case qml_mtx_events::Tag:
        return mtx::events::EventType::Tag;
    case qml_mtx_events::PolicyRuleUser:
        return mtx::events::EventType::PolicyRuleUser;
    case qml_mtx_events::PolicyRuleRoom:
        return mtx::events::EventType::PolicyRuleRoom;
    case qml_mtx_events::PolicyRuleServer:
        return mtx::events::EventType::PolicyRuleServer;
    // m.space.parent
    case qml_mtx_events::SpaceParent:
        return mtx::events::EventType::SpaceParent;
    // m.space.child
    case qml_mtx_events::SpaceChild:
        return mtx::events::EventType::SpaceChild;
    /// m.room.message
    case qml_mtx_events::AudioMessage:
    case qml_mtx_events::ElementEffectMessage:
    case qml_mtx_events::EmoteMessage:
    case qml_mtx_events::FileMessage:
    case qml_mtx_events::ImageMessage:
    case qml_mtx_events::LocationMessage:
    case qml_mtx_events::NoticeMessage:
    case qml_mtx_events::TextMessage:
    case qml_mtx_events::UnknownMessage:
    case qml_mtx_events::VideoMessage:
    case qml_mtx_events::Redacted:
    case qml_mtx_events::UnknownEvent:
    case qml_mtx_events::KeyVerificationRequest:
    case qml_mtx_events::KeyVerificationStart:
    case qml_mtx_events::KeyVerificationMac:
    case qml_mtx_events::KeyVerificationAccept:
    case qml_mtx_events::KeyVerificationCancel:
    case qml_mtx_events::KeyVerificationKey:
    case qml_mtx_events::KeyVerificationDone:
    case qml_mtx_events::KeyVerificationReady:
        return mtx::events::EventType::RoomMessage;
        //! m.image_pack, currently im.ponies.room_emotes
    case qml_mtx_events::ImagePackInRoom:
        return mtx::events::EventType::ImagePackInRoom;
    //! m.image_pack, currently im.ponies.user_emotes
    case qml_mtx_events::ImagePackInAccountData:
        return mtx::events::EventType::ImagePackInAccountData;
    //! m.image_pack.rooms, currently im.ponies.emote_rooms
    case qml_mtx_events::ImagePackRooms:
        return mtx::events::EventType::ImagePackRooms;
    default:
        return mtx::events::EventType::Unsupported;
    };
}

