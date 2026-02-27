// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cache/api/CacheApiWrappers.h"
#include "cache/core/Cache.h"
#include "cache/core/Cache_p.h"

//! Get a specific state event
template<typename T>
std::optional<mtx::events::StateEvent<T>>
Cache::getStateEvent(const std::string &room_id, std::string_view state_key)
{
    auto txn = beginTxn(nullptr, db::TransactionFlags::ReadOnly);
    return getStateEvent<T>(txn, room_id, state_key);
}
template<typename T>
std::vector<mtx::events::StateEvent<T>>
Cache::getStateEventsWithType(const std::string &room_id, mtx::events::EventType type)
{
    auto txn = beginTxn(nullptr, db::TransactionFlags::ReadOnly);
    return getStateEventsWithType<T>(txn, room_id, type);
}

#define KOMAI_CACHE_GET_STATE_EVENT_DEFINITION(Content)                                            \
    template std::optional<mtx::events::StateEvent<Content>> Cache::getStateEvent<Content>(        \
      const std::string &room_id, std::string_view state_key);

#define KOMAI_CACHE_GET_STATE_EVENTS_DEFINITION(Content)                                           \
    template std::vector<mtx::events::StateEvent<Content>> Cache::getStateEventsWithType<Content>( \
      const std::string &room_id, mtx::events::EventType type);

KOMAI_CACHE_GET_STATE_EVENT_DEFINITION(mtx::events::msc2545::ImagePack)
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

KOMAI_CACHE_GET_STATE_EVENTS_DEFINITION(mtx::events::state::Aliases)
KOMAI_CACHE_GET_STATE_EVENTS_DEFINITION(mtx::events::state::Avatar)
KOMAI_CACHE_GET_STATE_EVENTS_DEFINITION(mtx::events::state::CanonicalAlias)
KOMAI_CACHE_GET_STATE_EVENTS_DEFINITION(mtx::events::state::Create)
KOMAI_CACHE_GET_STATE_EVENTS_DEFINITION(mtx::events::state::Encryption)
KOMAI_CACHE_GET_STATE_EVENTS_DEFINITION(mtx::events::state::GuestAccess)
KOMAI_CACHE_GET_STATE_EVENTS_DEFINITION(mtx::events::state::HistoryVisibility)
KOMAI_CACHE_GET_STATE_EVENTS_DEFINITION(mtx::events::state::JoinRules)
KOMAI_CACHE_GET_STATE_EVENTS_DEFINITION(mtx::events::state::Member)
KOMAI_CACHE_GET_STATE_EVENTS_DEFINITION(mtx::events::state::Name)
KOMAI_CACHE_GET_STATE_EVENTS_DEFINITION(mtx::events::state::PinnedEvents)
KOMAI_CACHE_GET_STATE_EVENTS_DEFINITION(mtx::events::state::PowerLevels)
KOMAI_CACHE_GET_STATE_EVENTS_DEFINITION(mtx::events::state::Tombstone)
KOMAI_CACHE_GET_STATE_EVENTS_DEFINITION(mtx::events::state::ServerAcl)
KOMAI_CACHE_GET_STATE_EVENTS_DEFINITION(mtx::events::state::Topic)
KOMAI_CACHE_GET_STATE_EVENTS_DEFINITION(mtx::events::state::Widget)
KOMAI_CACHE_GET_STATE_EVENTS_DEFINITION(mtx::events::state::policy_rule::UserRule)
KOMAI_CACHE_GET_STATE_EVENTS_DEFINITION(mtx::events::state::policy_rule::RoomRule)
KOMAI_CACHE_GET_STATE_EVENTS_DEFINITION(mtx::events::state::policy_rule::ServerRule)
KOMAI_CACHE_GET_STATE_EVENTS_DEFINITION(mtx::events::state::space::Child)
KOMAI_CACHE_GET_STATE_EVENTS_DEFINITION(mtx::events::state::space::Parent)
KOMAI_CACHE_GET_STATE_EVENTS_DEFINITION(mtx::events::msc2545::ImagePack)

#undef KOMAI_CACHE_GET_STATE_EVENT_DEFINITION
#undef KOMAI_CACHE_GET_STATE_EVENTS_DEFINITION
