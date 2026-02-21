// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "db/MemberInfo.h"

#include <nlohmann/json.hpp>

#include "CacheStructs.h"
#include "db/DbTypes.h"

void
to_json(nlohmann::json &j, const MemberInfo &info)
{
    j["name"]       = info.name;
    j["avatar_url"] = info.avatar_url;
    if (!info.inviter.empty())
        j["inviter"] = info.inviter;
    if (info.is_direct)
        j["is_direct"] = info.is_direct;
    if (!info.reason.empty())
        j["reason"] = info.reason;
}

void
from_json(const nlohmann::json &j, MemberInfo &info)
{
    info.name       = j.value("name", "");
    info.avatar_url = j.value("avatar_url", "");
    info.is_direct  = j.value("is_direct", false);
    info.reason     = j.value("reason", "");
    info.inviter    = j.value("inviter", "");
}

namespace db {

std::string
serializeMemberInfo(const MemberInfo &info)
{
    return nlohmann::json(info).dump();
}

MemberInfo
parseMemberInfo(std::string_view value)
{
    return nlohmann::json::parse(value).get<MemberInfo>();
}

bool
getMemberInfo(Txn &txn, Dbi &membersDb, std::string_view userId, MemberInfo &info)
{
    std::string_view value;
    if (!membersDb.get(txn, userId, value))
        return false;

    info = parseMemberInfo(value);
    return true;
}

std::optional<MemberInfo>
getMemberInfo(Txn &txn, Dbi &membersDb, std::string_view userId)
{
    MemberInfo info;
    if (!getMemberInfo(txn, membersDb, userId, info))
        return std::nullopt;

    return info;
}

void
putMemberInfo(Txn &txn, Dbi &membersDb, std::string_view userId, const MemberInfo &info)
{
    membersDb.put(txn, userId, serializeMemberInfo(info));
}

} // namespace db
