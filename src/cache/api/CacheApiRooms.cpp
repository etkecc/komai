// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cache/api/CacheApiRooms.h"
#include "cache/api/CacheApiContext.h"
#include "cache/core/Cache_p.h"

namespace cache {

std::vector<RoomMember>
getMembers(const std::string &room_id, std::size_t startIndex, std::size_t len)
{
    return cacheInstance()->getMembers(room_id, startIndex, len);
}

std::vector<RoomMember>
getMembersFromInvite(const std::string &room_id, std::size_t startIndex, std::size_t len)
{
    return cacheInstance()->getMembersFromInvite(room_id, startIndex, len);
}

size_t
memberCount(const std::string &room_id)
{
    return cacheInstance()->memberCount(room_id);
}

template<typename T>
std::optional<mtx::events::StateEvent<T>>
getStateEvent(const std::string &room_id, std::string_view state_key)
{
    return cacheInstance()->getStateEvent<T>(room_id, state_key);
}

template<typename T>
std::vector<mtx::events::StateEvent<T>>
getStateEventsWithType(const std::string &room_id, mtx::events::EventType type)
{
    return cacheInstance()->getStateEventsWithType<T>(room_id, type);
}

#define KOMAI_CACHE_GET_STATE_EVENT_DEFINITION(Content)                                            \
    template std::optional<mtx::events::StateEvent<Content>> getStateEvent<Content>(               \
      const std::string &room_id, std::string_view state_key);                                     \
    template std::vector<mtx::events::StateEvent<Content>> getStateEventsWithType<Content>(        \
      const std::string &room_id, mtx::events::EventType type);

KOMAI_CACHE_GET_STATE_EVENT_DEFINITION(mtx::events::state::Aliases)
KOMAI_CACHE_GET_STATE_EVENT_DEFINITION(mtx::events::state::Avatar)
KOMAI_CACHE_GET_STATE_EVENT_DEFINITION(mtx::events::state::CanonicalAlias)
KOMAI_CACHE_GET_STATE_EVENT_DEFINITION(mtx::events::state::Create)
KOMAI_CACHE_GET_STATE_EVENT_DEFINITION(mtx::events::state::Encryption)
KOMAI_CACHE_GET_STATE_EVENT_DEFINITION(mtx::events::state::GuestAccess)
KOMAI_CACHE_GET_STATE_EVENT_DEFINITION(mtx::events::state::HistoryVisibility)
KOMAI_CACHE_GET_STATE_EVENT_DEFINITION(mtx::events::state::JoinRules)
KOMAI_CACHE_GET_STATE_EVENT_DEFINITION(mtx::events::state::Member)
KOMAI_CACHE_GET_STATE_EVENT_DEFINITION(mtx::events::state::Name)
KOMAI_CACHE_GET_STATE_EVENT_DEFINITION(mtx::events::state::PinnedEvents)
KOMAI_CACHE_GET_STATE_EVENT_DEFINITION(mtx::events::state::PowerLevels)
KOMAI_CACHE_GET_STATE_EVENT_DEFINITION(mtx::events::state::Tombstone)
KOMAI_CACHE_GET_STATE_EVENT_DEFINITION(mtx::events::state::ServerAcl)
KOMAI_CACHE_GET_STATE_EVENT_DEFINITION(mtx::events::state::Topic)
KOMAI_CACHE_GET_STATE_EVENT_DEFINITION(mtx::events::state::Widget)
KOMAI_CACHE_GET_STATE_EVENT_DEFINITION(mtx::events::state::policy_rule::UserRule)
KOMAI_CACHE_GET_STATE_EVENT_DEFINITION(mtx::events::state::policy_rule::RoomRule)
KOMAI_CACHE_GET_STATE_EVENT_DEFINITION(mtx::events::state::policy_rule::ServerRule)
KOMAI_CACHE_GET_STATE_EVENT_DEFINITION(mtx::events::state::space::Child)
KOMAI_CACHE_GET_STATE_EVENT_DEFINITION(mtx::events::state::space::Parent)
KOMAI_CACHE_GET_STATE_EVENT_DEFINITION(mtx::events::msc2545::ImagePack)

#undef KOMAI_CACHE_GET_STATE_EVENT_DEFINITION

bool
isRoomMember(const std::string &user_id, const std::string &room_id)
{
    return cacheInstance()->isRoomMember(user_id, room_id);
}

std::vector<QString>
roomIds()
{
    return cacheInstance()->roomIds();
}

} // namespace cache
