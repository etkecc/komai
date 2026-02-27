// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cache/Cache.h"
#include "cache/core/Cache_p.h"

#include <map>
#include <string_view>

#include <nlohmann/json.hpp>
#include <spdlog/logger.h>

#include "cache/api/CacheApiContext.h"
#include "db/RoomInfo.h"

using namespace mtx::events;

QString
Cache::getRoomAvatarUrl(db::Transaction &txn, db::Store &statesdb, db::Store &membersdb)
{
    using namespace mtx::events::state;

    try {
        if (auto msg = db::getJsonValue<StateEvent<Avatar>>(
              txn, statesdb, to_string(mtx::events::EventType::RoomAvatar))) {
            if (!msg->content.url.empty())
                return QString::fromStdString(msg->content.url);
        }
    } catch (const nlohmann::json::exception &e) {
        cache::activeLoggers().db->warn("failed to parse m.room.avatar event: {}", e.what());
    }

    // We don't use an avatar for group chats.
    if (membersdb.size(txn) > 2)
        return QString();

    const auto localUserId = localUserId_.toStdString();
    std::string fallback_url;
    std::string direct_url;
    bool foundDirectUrl = false;

    // Resolve avatar for 1-1 chats.
    db::forEachEntry(txn, membersdb, [&](std::string_view user_id, std::string_view member_data) {
        try {
            MemberInfo m = db::parseMemberInfo(member_data);
            if (user_id == localUserId) {
                fallback_url = m.avatar_url;
                return true;
            }

            direct_url     = m.avatar_url;
            foundDirectUrl = true;
            return false;
        } catch (const nlohmann::json::exception &e) {
            cache::activeLoggers().db->warn("failed to parse member info: {}", e.what());
        }
        return true;
    });

    if (foundDirectUrl)
        return QString::fromStdString(direct_url);

    // Default case when there is only one member.
    return QString::fromStdString(fallback_url);
}

QString
Cache::getRoomName(db::Transaction &txn, db::Store &statesdb, db::Store &membersdb)
{
    using namespace mtx::events;
    using namespace mtx::events::state;

    try {
        if (auto msg = db::getJsonValue<StateEvent<Name>>(
              txn, statesdb, to_string(mtx::events::EventType::RoomName))) {
            if (!msg->content.name.empty())
                return QString::fromStdString(msg->content.name);
        }
    } catch (const nlohmann::json::exception &e) {
        cache::activeLoggers().db->warn("failed to parse m.room.name event: {}", e.what());
    }

    try {
        if (auto msg = db::getJsonValue<StateEvent<CanonicalAlias>>(
              txn, statesdb, to_string(mtx::events::EventType::RoomCanonicalAlias))) {
            if (!msg->content.alias.empty())
                return QString::fromStdString(msg->content.alias);
        }
    } catch (const nlohmann::json::exception &e) {
        cache::activeLoggers().db->warn("failed to parse m.room.canonical_alias event: {}",
                                        e.what());
    }

    const auto total = membersdb.size(txn);

    std::map<std::string, MemberInfo> members;

    db::forEachEntry(
      txn, membersdb, 0, 3, [&members](std::string_view user_id, std::string_view member_data) {
          try {
              members.emplace(user_id, db::parseMemberInfo(member_data));
          } catch (const nlohmann::json::exception &e) {
              cache::activeLoggers().db->warn("failed to parse member info: {}", e.what());
          }
          return true;
      });

    if (total == 1 && !members.empty())
        return QString::fromStdString(members.begin()->second.name);

    auto first_member = [&members, this]() {
        for (const auto &m : members) {
            if (m.first != localUserId_.toStdString())
                return QString::fromStdString(m.second.name);
        }

        return localUserId_;
    }();
    auto second_member = [&members, this]() {
        bool first = true;
        for (const auto &m : members) {
            if (m.first != localUserId_.toStdString()) {
                if (first)
                    first = false;
                else
                    return QString::fromStdString(m.second.name);
            }
        }

        return localUserId_;
    }();

    if (total == 2)
        return first_member;
    else if (total == 3)
        return tr("%1 and %2", "RoomName").arg(first_member, second_member);
    else if (total > 3)
        return tr("%1 and %n other(s)", "", (int)total - 2).arg(first_member);

    return tr("Empty Room");
}

mtx::events::state::JoinRule
Cache::getRoomJoinRule(db::Transaction &txn, db::Store &statesdb)
{
    using namespace mtx::events;
    using namespace mtx::events::state;

    try {
        if (auto msg = db::getJsonValue<StateEvent<state::JoinRules>>(
              txn, statesdb, to_string(mtx::events::EventType::RoomJoinRules))) {
            return msg->content.join_rule;
        }
    } catch (const nlohmann::json::exception &e) {
        cache::activeLoggers().db->warn("failed to parse m.room.join_rule event: {}", e.what());
    }
    return state::JoinRule::Knock;
}

bool
Cache::getRoomGuestAccess(db::Transaction &txn, db::Store &statesdb)
{
    using namespace mtx::events;
    using namespace mtx::events::state;

    try {
        if (auto msg = db::getJsonValue<StateEvent<GuestAccess>>(
              txn, statesdb, to_string(mtx::events::EventType::RoomGuestAccess))) {
            return msg->content.guest_access == AccessState::CanJoin;
        }
    } catch (const nlohmann::json::exception &e) {
        cache::activeLoggers().db->warn("failed to parse m.room.guest_access event: {}", e.what());
    }
    return false;
}

QString
Cache::getRoomTopic(db::Transaction &txn, db::Store &statesdb)
{
    using namespace mtx::events;
    using namespace mtx::events::state;

    try {
        if (auto msg = db::getJsonValue<StateEvent<Topic>>(
              txn, statesdb, to_string(mtx::events::EventType::RoomTopic))) {
            if (!msg->content.topic.empty())
                return QString::fromStdString(msg->content.topic);
        }
    } catch (const nlohmann::json::exception &e) {
        cache::activeLoggers().db->warn("failed to parse m.room.topic event: {}", e.what());
    }

    return QString();
}

QString
Cache::getRoomVersion(db::Transaction &txn, db::Store &statesdb)
{
    using namespace mtx::events;
    using namespace mtx::events::state;

    try {
        if (auto msg = db::getJsonValue<StateEvent<Create>>(
              txn, statesdb, to_string(mtx::events::EventType::RoomCreate))) {
            if (!msg->content.room_version.empty())
                return QString::fromStdString(msg->content.room_version);
        }
    } catch (const nlohmann::json::exception &e) {
        cache::activeLoggers().db->warn("failed to parse m.room.create event: {}", e.what());
    }

    cache::activeLoggers().db->warn(
      "m.room.create event is missing room version, assuming version \"1\"");
    return QStringLiteral("1");
}

bool
Cache::getRoomIsSpace(db::Transaction &txn, db::Store &statesdb)
{
    using namespace mtx::events;
    using namespace mtx::events::state;

    try {
        if (auto msg = db::getJsonValue<StateEvent<Create>>(
              txn, statesdb, to_string(mtx::events::EventType::RoomCreate))) {
            return msg->content.type == mtx::events::state::room_type::space;
        }
    } catch (const nlohmann::json::exception &e) {
        cache::activeLoggers().db->warn("failed to parse m.room.create event: {}", e.what());
    }

    cache::activeLoggers().db->warn(
      "m.room.create event is missing room version, assuming version \"1\"");
    return false;
}

bool
Cache::getRoomIsTombstoned(db::Transaction &txn, db::Store &statesdb)
{
    using namespace mtx::events;
    using namespace mtx::events::state;

    try {
        if (auto msg = db::getJsonValue<StateEvent<Tombstone>>(
              txn, statesdb, to_string(mtx::events::EventType::RoomCreate))) {
            return true;
        }
    } catch (const nlohmann::json::exception &e) {
        cache::activeLoggers().db->warn("failed to parse m.room.tombstone event: {}", e.what());
    }

    return false;
}
