// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Cache.h"
#include "Cache_p.h"
#include "CacheApiWrappers.h"

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

#define NHEKO_CACHE_GET_STATE_EVENT_DEFINITION(Content)                                            \
    template std::optional<mtx::events::StateEvent<Content>> Cache::getStateEvent<Content>(        \
      const std::string &room_id, std::string_view state_key);

#define NHEKO_CACHE_GET_STATE_EVENTS_DEFINITION(Content)                                           \
    template std::vector<mtx::events::StateEvent<Content>> Cache::getStateEventsWithType<Content>( \
      const std::string &room_id, mtx::events::EventType type);

NHEKO_CACHE_GET_STATE_EVENT_DEFINITION(mtx::events::msc2545::ImagePack)
NHEKO_CACHE_GET_STATE_EVENT_DEFINITION(mtx::events::state::Aliases)
NHEKO_CACHE_GET_STATE_EVENT_DEFINITION(mtx::events::state::Avatar)
NHEKO_CACHE_GET_STATE_EVENT_DEFINITION(mtx::events::state::CanonicalAlias)
NHEKO_CACHE_GET_STATE_EVENT_DEFINITION(mtx::events::state::Create)
NHEKO_CACHE_GET_STATE_EVENT_DEFINITION(mtx::events::state::Encryption)
NHEKO_CACHE_GET_STATE_EVENT_DEFINITION(mtx::events::state::GuestAccess)
NHEKO_CACHE_GET_STATE_EVENT_DEFINITION(mtx::events::state::HistoryVisibility)
NHEKO_CACHE_GET_STATE_EVENT_DEFINITION(mtx::events::state::JoinRules)
NHEKO_CACHE_GET_STATE_EVENT_DEFINITION(mtx::events::state::Member)
NHEKO_CACHE_GET_STATE_EVENT_DEFINITION(mtx::events::state::Name)
NHEKO_CACHE_GET_STATE_EVENT_DEFINITION(mtx::events::state::PinnedEvents)
NHEKO_CACHE_GET_STATE_EVENT_DEFINITION(mtx::events::state::PowerLevels)
NHEKO_CACHE_GET_STATE_EVENT_DEFINITION(mtx::events::state::Tombstone)
NHEKO_CACHE_GET_STATE_EVENT_DEFINITION(mtx::events::state::ServerAcl)
NHEKO_CACHE_GET_STATE_EVENT_DEFINITION(mtx::events::state::Topic)
NHEKO_CACHE_GET_STATE_EVENT_DEFINITION(mtx::events::state::Widget)
NHEKO_CACHE_GET_STATE_EVENT_DEFINITION(mtx::events::state::policy_rule::UserRule)
NHEKO_CACHE_GET_STATE_EVENT_DEFINITION(mtx::events::state::policy_rule::RoomRule)
NHEKO_CACHE_GET_STATE_EVENT_DEFINITION(mtx::events::state::policy_rule::ServerRule)
NHEKO_CACHE_GET_STATE_EVENT_DEFINITION(mtx::events::state::space::Child)
NHEKO_CACHE_GET_STATE_EVENT_DEFINITION(mtx::events::state::space::Parent)

NHEKO_CACHE_GET_STATE_EVENTS_DEFINITION(mtx::events::state::Aliases)
NHEKO_CACHE_GET_STATE_EVENTS_DEFINITION(mtx::events::state::Avatar)
NHEKO_CACHE_GET_STATE_EVENTS_DEFINITION(mtx::events::state::CanonicalAlias)
NHEKO_CACHE_GET_STATE_EVENTS_DEFINITION(mtx::events::state::Create)
NHEKO_CACHE_GET_STATE_EVENTS_DEFINITION(mtx::events::state::Encryption)
NHEKO_CACHE_GET_STATE_EVENTS_DEFINITION(mtx::events::state::GuestAccess)
NHEKO_CACHE_GET_STATE_EVENTS_DEFINITION(mtx::events::state::HistoryVisibility)
NHEKO_CACHE_GET_STATE_EVENTS_DEFINITION(mtx::events::state::JoinRules)
NHEKO_CACHE_GET_STATE_EVENTS_DEFINITION(mtx::events::state::Member)
NHEKO_CACHE_GET_STATE_EVENTS_DEFINITION(mtx::events::state::Name)
NHEKO_CACHE_GET_STATE_EVENTS_DEFINITION(mtx::events::state::PinnedEvents)
NHEKO_CACHE_GET_STATE_EVENTS_DEFINITION(mtx::events::state::PowerLevels)
NHEKO_CACHE_GET_STATE_EVENTS_DEFINITION(mtx::events::state::Tombstone)
NHEKO_CACHE_GET_STATE_EVENTS_DEFINITION(mtx::events::state::ServerAcl)
NHEKO_CACHE_GET_STATE_EVENTS_DEFINITION(mtx::events::state::Topic)
NHEKO_CACHE_GET_STATE_EVENTS_DEFINITION(mtx::events::state::Widget)
NHEKO_CACHE_GET_STATE_EVENTS_DEFINITION(mtx::events::state::policy_rule::UserRule)
NHEKO_CACHE_GET_STATE_EVENTS_DEFINITION(mtx::events::state::policy_rule::RoomRule)
NHEKO_CACHE_GET_STATE_EVENTS_DEFINITION(mtx::events::state::policy_rule::ServerRule)
NHEKO_CACHE_GET_STATE_EVENTS_DEFINITION(mtx::events::state::space::Child)
NHEKO_CACHE_GET_STATE_EVENTS_DEFINITION(mtx::events::state::space::Parent)
NHEKO_CACHE_GET_STATE_EVENTS_DEFINITION(mtx::events::msc2545::ImagePack)

#undef NHEKO_CACHE_GET_STATE_EVENT_DEFINITION
#undef NHEKO_CACHE_GET_STATE_EVENTS_DEFINITION
