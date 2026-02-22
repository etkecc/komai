// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Cache.h"
#include "Cache_p.h"
#include "CacheApiWrappers.h"

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

#define NHEKO_CACHE_GET_STATE_EVENT_DEFINITION(Content)                                            \
    template std::optional<mtx::events::StateEvent<Content>> getStateEvent<Content>(        \
      const std::string &room_id, std::string_view state_key);                                     \
    template std::vector<mtx::events::StateEvent<Content>> getStateEventsWithType<Content>( \
      const std::string &room_id, mtx::events::EventType type);

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
NHEKO_CACHE_GET_STATE_EVENT_DEFINITION(mtx::events::msc2545::ImagePack)

#undef NHEKO_CACHE_GET_STATE_EVENT_DEFINITION

void
saveState(const mtx::responses::Sync &res)
{
    cacheInstance()->saveState(res);
}
void
updateState(const std::string &room, const mtx::responses::StateEvents &state, bool wipe)
{
    cacheInstance()->updateState(room, state, wipe);
}
bool
isInitialized()
{
    return cacheInstance()->isInitialized();
}

std::string
nextBatchToken()
{
    return cacheInstance()->nextBatchToken();
}
std::string
previousBatchToken(const std::string &room_id)
{
    return cacheInstance()->previousBatchToken(room_id);
}

void
deleteData()
{
    cacheInstance()->deleteData();
}

void
removeInvite(const std::string &room_id)
{
    cacheInstance()->removeInvite(room_id);
}
void
removeRoom(const std::string &roomid)
{
    cacheInstance()->removeRoom(roomid);
}
void
removeRoom(const QString &roomid)
{
    cacheInstance()->removeRoom(roomid.toStdString());
}
void
setup()
{
    cacheInstance()->setup();
}

bool
runMigrations()
{
    return cacheInstance()->runMigrations();
}

cache::CacheVersion
formatVersion()
{
    return cacheInstance()->formatVersion();
}

void
setCurrentFormat()
{
    cacheInstance()->setCurrentFormat();
}

std::vector<QString>
roomIds()
{
    return cacheInstance()->roomIds();
}

} // namespace cache
