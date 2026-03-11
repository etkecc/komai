// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cache/api/CacheApiContext.h"
#include "cache/api/CacheApiLifecycle.h"
#include "cache/core/Cache_p.h"

namespace cache {

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
storageBackendId()
{
    if (!cacheInstance())
        return {};

    return cacheInstance()->storageBackendId();
}

bool
storageSupportsCompaction() noexcept
{
    if (!cacheInstance())
        return false;

    return cacheInstance()->storageSupportsCompaction();
}

std::optional<std::size_t>
storageMapSizeBytes() noexcept
{
    if (!cacheInstance())
        return std::nullopt;

    return cacheInstance()->storageMapSizeBytes();
}

std::size_t
namedStoreCount()
{
    if (!cacheInstance())
        return 0;

    return cacheInstance()->namedStoreCount();
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

void
deleteOldMessages()
{
    cacheInstance()->deleteOldMessages();
}

void
deleteOldData() noexcept
{
    cacheInstance()->deleteOldData();
}

} // namespace cache
