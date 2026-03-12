// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "cache/api/CacheApiTypes.h"

namespace cache {
QMap<QString, RoomInfo>
roomInfo(bool withInvites = true);
QHash<QString, RoomInfo>
invites();
std::optional<mtx::events::collections::RoomAccountDataEvents>
getAccountData(mtx::events::EventType type, const std::string &room_id = "");
std::optional<std::string>
getAccountDataByType(const std::string &type, const std::string &room_id = "");
std::vector<RoomNameAlias>
roomNamesAndAliases();
std::vector<QString>
roomIds();
std::optional<RoomInfo>
invite(std::string_view roomid);
std::optional<MemberInfo>
getInviteMember(const std::string &room_id, const std::string &user_id);
std::vector<std::string>
getParentRoomIds(const std::string &room_id);
std::vector<std::string>
getChildRoomIds(const std::string &room_id);

//! Retrieve member info from a room.
std::vector<RoomMember>
getMembers(const std::string &room_id, std::size_t startIndex = 0, std::size_t len = 30);
//! Retrive member info from an invite.
std::vector<RoomMember>
getMembersFromInvite(const std::string &room_id, std::size_t start_index = 0, std::size_t len = 30);
size_t
memberCount(const std::string &room_id);

template<typename T>
std::optional<mtx::events::StateEvent<T>>
getStateEvent(const std::string &room_id, std::string_view state_key = "");
template<typename T>
std::vector<mtx::events::StateEvent<T>>
getStateEventsWithType(const std::string &room_id,
                       mtx::events::EventType type = mtx::events::state_content_to_type<T>);

//! Retrieve all the user ids from a room.
std::vector<std::string>
roomMembers(const std::string &room_id);

//! Check if the given user is treated as a room-v12 creator.
bool
isV12Creator(const std::string &room_id, const std::string &user_id);

//! Check if the given user has power level greater than than
//! lowest power level of the given events.
bool
hasEnoughPowerLevel(const std::vector<mtx::events::EventType> &eventTypes,
                    const std::string &room_id,
                    const std::string &user_id);

//! Adds a user to the read list for the given event.
//!
//! There should be only one user id present in a receipt list per room.
//! The user id should be removed from any other lists.
using UserReceipts = std::multimap<uint64_t, std::string, std::greater<uint64_t>>;
UserReceipts
readReceipts(const QString &event_id, const QString &room_id);

RoomInfo
singleRoomInfo(const std::string &room_id);
std::map<QString, RoomInfo>
getRoomInfo(const std::vector<std::string> &rooms);
QString
roomAvatarUrl(const std::string &room_id);

//! Calculates which the read status of a room.
//! Whether all the events in the timeline have been read.
std::string
getFullyReadEventId(const std::string &room_id);
bool
calculateRoomReadStatus(const std::string &room_id);
void
calculateRoomReadStatus();
void
updateLastMessageTimestamp(const std::string &room_id, uint64_t ts);
crypto::Trust
roomVerificationStatus(const std::string &room_id);

bool
isRoomEncrypted(const std::string &room_id);
std::optional<mtx::events::state::Encryption>
roomEncryptionSettings(const std::string &room_id);
std::map<std::string, std::optional<UserKeyCache>>
getMembersWithKeys(const std::string &room_id, bool verified_only);

//! Check if a user is a member of the room.
bool
isRoomMember(const std::string &user_id, const std::string &room_id);

std::vector<ImagePackInfo>
getImagePacks(const std::string &room_id, std::optional<bool> stickers);

#define KOMAI_CACHE_GET_STATE_EVENT_WRAPPER_FORWARD(Content)                                       \
    extern template std::optional<mtx::events::StateEvent<Content>> cache::getStateEvent<Content>( \
      const std::string &room_id, std::string_view state_key);                                     \
                                                                                                   \
    extern template std::vector<mtx::events::StateEvent<Content>>                                  \
    cache::getStateEventsWithType<Content>(const std::string &room_id,                             \
                                           mtx::events::EventType type);

KOMAI_CACHE_GET_STATE_EVENT_WRAPPER_FORWARD(mtx::events::state::Aliases)
KOMAI_CACHE_GET_STATE_EVENT_WRAPPER_FORWARD(mtx::events::state::Avatar)
KOMAI_CACHE_GET_STATE_EVENT_WRAPPER_FORWARD(mtx::events::state::CanonicalAlias)
KOMAI_CACHE_GET_STATE_EVENT_WRAPPER_FORWARD(mtx::events::state::Create)
KOMAI_CACHE_GET_STATE_EVENT_WRAPPER_FORWARD(mtx::events::state::Encryption)
KOMAI_CACHE_GET_STATE_EVENT_WRAPPER_FORWARD(mtx::events::state::GuestAccess)
KOMAI_CACHE_GET_STATE_EVENT_WRAPPER_FORWARD(mtx::events::state::HistoryVisibility)
KOMAI_CACHE_GET_STATE_EVENT_WRAPPER_FORWARD(mtx::events::state::JoinRules)
KOMAI_CACHE_GET_STATE_EVENT_WRAPPER_FORWARD(mtx::events::state::Member)
KOMAI_CACHE_GET_STATE_EVENT_WRAPPER_FORWARD(mtx::events::state::Name)
KOMAI_CACHE_GET_STATE_EVENT_WRAPPER_FORWARD(mtx::events::state::PinnedEvents)
KOMAI_CACHE_GET_STATE_EVENT_WRAPPER_FORWARD(mtx::events::state::PowerLevels)
KOMAI_CACHE_GET_STATE_EVENT_WRAPPER_FORWARD(mtx::events::state::Tombstone)
KOMAI_CACHE_GET_STATE_EVENT_WRAPPER_FORWARD(mtx::events::state::ServerAcl)
KOMAI_CACHE_GET_STATE_EVENT_WRAPPER_FORWARD(mtx::events::state::Topic)
KOMAI_CACHE_GET_STATE_EVENT_WRAPPER_FORWARD(mtx::events::state::Widget)
KOMAI_CACHE_GET_STATE_EVENT_WRAPPER_FORWARD(mtx::events::state::policy_rule::UserRule)
KOMAI_CACHE_GET_STATE_EVENT_WRAPPER_FORWARD(mtx::events::state::policy_rule::RoomRule)
KOMAI_CACHE_GET_STATE_EVENT_WRAPPER_FORWARD(mtx::events::state::policy_rule::ServerRule)
KOMAI_CACHE_GET_STATE_EVENT_WRAPPER_FORWARD(mtx::events::state::space::Child)
KOMAI_CACHE_GET_STATE_EVENT_WRAPPER_FORWARD(mtx::events::state::space::Parent)
KOMAI_CACHE_GET_STATE_EVENT_WRAPPER_FORWARD(mtx::events::msc2545::ImagePack)

#undef KOMAI_CACHE_GET_STATE_EVENT_WRAPPER_FORWARD
} // namespace cache
