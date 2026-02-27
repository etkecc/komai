// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cache/api/CacheApiContext.h"
#include "cache/api/CacheApiUsers.h"
#include "cache/core/Cache_p.h"

#include <utility>
#include <vector>

namespace cache {

std::string
displayName(const std::string &room_id, const std::string &user_id)
{
    return cacheInstance()->displayName(room_id, user_id);
}

QString
displayName(const QString &room_id, const QString &user_id)
{
    return cacheInstance()->displayName(room_id, user_id);
}

QString
avatarUrl(const QString &room_id, const QString &user_id)
{
    return cacheInstance()->avatarUrl(room_id, user_id);
}

mtx::events::presence::Presence
presence(const std::string &user_id)
{
    if (!cacheInstance())
        return {};

    return cacheInstance()->presence(user_id);
}

std::optional<UserKeyCache>
userKeys(const std::string &user_id)
{
    return cacheInstance()->userKeys(user_id);
}

std::map<std::string, RoomInfo>
getCommonRooms(const std::string &user_id)
{
    return cacheInstance()->getCommonRooms(user_id);
}

void
markUserKeysOutOfDate(const std::vector<std::string> &user_ids)
{
    cacheInstance()->markUserKeysOutOfDate(user_ids);
}

void
queryKeys(
  const std::string &user_id,
  std::function<void(const UserKeyCache &, const std::optional<mtx::http::ClientError> &)> callback)
{
    cacheInstance()->query_keys(user_id, std::move(callback));
}

void
updateUserKeys(const std::string &sync_token, const mtx::responses::QueryKeys &keyQuery)
{
    cacheInstance()->updateUserKeys(sync_token, keyQuery);
}

std::optional<VerificationStatus>
verificationStatus(const std::string &user_id)
{
    return cacheInstance()->verificationStatus(user_id);
}

void
markDeviceVerified(const std::string &user_id, const std::string &device)
{
    cacheInstance()->markDeviceVerified(user_id, device);
}

void
markDeviceUnverified(const std::string &user_id, const std::string &device)
{
    cacheInstance()->markDeviceUnverified(user_id, device);
}

std::vector<std::string>
joinedRooms()
{
    return cacheInstance()->joinedRooms();
}

} // namespace cache
