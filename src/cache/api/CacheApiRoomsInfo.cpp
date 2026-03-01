// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cache/api/CacheApiContext.h"
#include "cache/api/CacheApiRooms.h"
#include "cache/core/Cache_p.h"

namespace cache {

QMap<QString, RoomInfo>
roomInfo(bool withInvites)
{
    return cacheInstance()->roomInfo(withInvites);
}

QHash<QString, RoomInfo>
invites()
{
    return cacheInstance()->invites();
}

std::optional<mtx::events::collections::RoomAccountDataEvents>
getAccountData(mtx::events::EventType type, const std::string &room_id)
{
    return cacheInstance()->getAccountData(type, room_id);
}

std::optional<std::string>
getAccountDataByType(const std::string &type, const std::string &room_id)
{
    return cacheInstance()->getAccountDataByType(type, room_id);
}

std::vector<RoomNameAlias>
roomNamesAndAliases()
{
    return cacheInstance()->roomNamesAndAliases();
}

std::optional<RoomInfo>
invite(std::string_view roomid)
{
    return cacheInstance()->invite(roomid);
}

std::optional<MemberInfo>
getInviteMember(const std::string &room_id, const std::string &user_id)
{
    return cacheInstance()->getInviteMember(room_id, user_id);
}

std::vector<std::string>
getParentRoomIds(const std::string &room_id)
{
    return cacheInstance()->getParentRoomIds(room_id);
}

std::vector<std::string>
getChildRoomIds(const std::string &room_id)
{
    return cacheInstance()->getChildRoomIds(room_id);
}

} // namespace cache
