// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "TimelineEventTypes.h"

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

std::vector<mtx::events::EventType>
qml_mtx_events::defaultHiddenEventTypes()
{
    using mtx::events::EventType;
    return {
      EventType::Reaction,
      EventType::CallCandidates,
      EventType::CallNegotiate,
      EventType::Unsupported,
      EventType::CallSelectAnswer,
    };
}
