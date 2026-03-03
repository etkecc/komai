// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "db/RoomInfo.h"

#include <cstddef>
#include <cstdint>
#include <stdexcept>

#include <QCoreApplication>

#include <nlohmann/json.hpp>

#include "db/DbTypes.h"
#include "db/Json.h"
#include "matrix/MatrixStateTypes.h"

void
to_json(nlohmann::json &j, const RoomInfo &info)
{
    j["name"]         = info.name;
    j["topic"]        = info.topic;
    j["avatar_url"]   = info.avatar_url;
    j["version"]      = info.version;
    j["is_invite"]    = info.is_invite;
    j["is_space"]     = info.is_space;
    j["tombst"]       = info.is_tombstoned;
    j["join_rule"]    = info.join_rule;
    j["guest_access"] = info.guest_access;

    j["app_l_ts"] = info.approximate_last_modification_ts;

    j["notification_count"] = info.notification_count;
    j["highlight_count"]    = info.highlight_count;

    if (info.member_count != 0)
        j["member_count"] = info.member_count;

    if (!info.tags.empty())
        j["tags"] = info.tags;
}

void
from_json(const nlohmann::json &j, RoomInfo &info)
{
    info.name       = j.at("name").get<std::string>();
    info.topic      = j.at("topic").get<std::string>();
    info.avatar_url = j.at("avatar_url").get<std::string>();
    info.version    = j.value(
      "version", QCoreApplication::translate("RoomInfo", "no version stored").toStdString());

    info.is_invite     = j.at("is_invite").get<bool>();
    info.is_space      = j.value("is_space", false);
    info.is_tombstoned = j.value("tombst", false);

    info.join_rule    = j.at("join_rule").get<mtx::events::state::JoinRule>();
    info.guest_access = j.at("guest_access").get<bool>();

    info.approximate_last_modification_ts = j.value<std::uint64_t>("app_l_ts", 0);
    // Work around bad values stored in older cache entries.
    if (info.approximate_last_modification_ts < 100000000000ULL)
        info.approximate_last_modification_ts = 0;

    info.notification_count = j.value("notification_count", 0ULL);
    info.highlight_count    = j.value("highlight_count", 0ULL);

    if (j.count("member_count"))
        info.member_count = j.at("member_count").get<std::size_t>();

    if (j.count("tags"))
        info.tags = j.at("tags").get<std::vector<std::string>>();
}

namespace db {

std::string
serializeRoomInfo(const RoomInfo &info)
{
    return nlohmann::json(info).dump();
}

RoomInfo
parseRoomInfo(std::string_view value)
{
    const auto parsed = db::parseJsonValue<RoomInfo>(value);
    if (!parsed)
        throw std::runtime_error("failed to parse room info");

    return *parsed;
}

bool
getRoomInfo(Txn &txn, Store &roomInfoDb, std::string_view roomId, RoomInfo &info)
{
    std::string_view value;
    if (!roomInfoDb.get(txn, roomId, value))
        return false;

    info = parseRoomInfo(value);
    return true;
}

std::optional<RoomInfo>
getRoomInfo(Txn &txn, Store &roomInfoDb, std::string_view roomId)
{
    RoomInfo info;
    if (!getRoomInfo(txn, roomInfoDb, roomId, info))
        return std::nullopt;

    return info;
}

void
putRoomInfo(Txn &txn, Store &roomInfoDb, std::string_view roomId, const RoomInfo &info)
{
    roomInfoDb.put(txn, roomId, serializeRoomInfo(info));
}

} // namespace db
