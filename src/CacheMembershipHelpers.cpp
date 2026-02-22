// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Cache.h"
#include "Cache_p.h"

#include <algorithm>
#include <limits>

#include <nlohmann/json.hpp>

#include <QHash>

#include "Logging.h"
#include "db/MemberInfo.h"

bool
Cache::hasEnoughPowerLevel(const std::vector<mtx::events::EventType> &eventTypes,
                           const std::string &room_id,
                           const std::string &user_id)
{
    using namespace mtx::events;
    using namespace mtx::events::state;

    auto txn = ro_txn(storage());
    try {
        auto db_ = getStatesDb(txn, room_id);

        int64_t min_event_level = std::numeric_limits<int64_t>::max();
        int64_t user_level      = std::numeric_limits<int64_t>::min();

        try {
            if (auto msg = db::getJsonValue<StateEvent<PowerLevels>>(
                  txn, db_, to_string(EventType::RoomPowerLevels))) {
                user_level = msg->content.user_level(user_id);

                for (const auto &ty : eventTypes)
                    min_event_level =
                      std::min(min_event_level, msg->content.state_level(to_string(ty)));
            }
        } catch (const nlohmann::json::exception &e) {
            nhlog::db()->warn("failed to parse m.room.power_levels event: {}", e.what());
        }

        return user_level >= min_event_level;
    } catch (...) {
        return false;
    }
}

std::vector<std::string>
Cache::roomMembers(const std::string &room_id)
{
    auto txn = ro_txn(storage());

    try {
        auto db_ = getMembersDb(txn, room_id);
        return db::listUniqueKeys(txn, db_);
    } catch (const db::Error &e) {
        nhlog::db()->error("Failed to retrieve members from db in room {}: {}", room_id, e.what());
        return {};
    }
}

size_t
Cache::memberCount(const std::string &room_id)
{
    auto txn = ro_txn(storage());
    return getMembersDb(txn, room_id).size(txn);
}

std::vector<std::string>
Cache::joinedRooms()
{
    auto txn = ro_txn(storage());
    return db::listUniqueKeys(txn, db->rooms);
}

std::map<std::string, RoomInfo>
Cache::getCommonRooms(const std::string &user_id)
{
    std::map<std::string, RoomInfo> result;

    auto txn = ro_txn(storage());

    std::string_view member_info;

    db::forEachEntry(
      txn,
      db->rooms,
      [this, &txn, &result, &user_id, &member_info](std::string_view room_id,
                                                    std::string_view room_data) {
          try {
              if (getMembersDb(txn, std::string(room_id)).get(txn, user_id, member_info)) {
                  RoomInfo tmp = db::parseRoomInfo(room_data);
                  result.emplace(std::string(room_id), std::move(tmp));
              }
          } catch (std::exception &e) {
              nhlog::db()->warn("Failed to read common room for member ({}) in room ({}): {}",
                                user_id,
                                room_id,
                                e.what());
          }
          return true;
      });

    return result;
}

std::optional<MemberInfo>
Cache::getMember(const std::string &room_id, const std::string &user_id)
{
    if (user_id.empty() || !db::isOpen(storage()))
        return std::nullopt;

    try {
        auto txn = ro_txn(storage());

        auto membersdb = getMembersDb(txn, room_id);

        return db::getMemberInfo(txn, membersdb, user_id);
    } catch (std::exception &e) {
        nhlog::db()->warn(
          "Failed to read member ({}) in room ({}): {}", user_id, room_id, e.what());
    }
    return std::nullopt;
}

std::vector<RoomMember>
Cache::getMembers(const std::string &room_id, std::size_t startIndex, std::size_t len)
{
    try {
        auto txn = ro_txn(storage());
        auto db_ = getMembersDb(txn, room_id);

        std::vector<RoomMember> members;

        db::forEachEntry(txn,
                         db_,
                         startIndex,
                         len,
                         [&members](std::string_view user_id, std::string_view user_data) {
                             try {
                                 MemberInfo tmp = db::parseMemberInfo(user_data);
                                 members.emplace_back(RoomMember{
                                   QString::fromStdString(std::string(user_id)),
                                   QString::fromStdString(tmp.name),
                                   QString::fromStdString(tmp.avatar_url),
                                 });
                             } catch (const nlohmann::json::exception &e) {
                                 nhlog::db()->warn("{}", e.what());
                             }
                             return true;
                         });

        return members;
    } catch (const db::Error &e) {
        nhlog::db()->error("Failed to retrieve members from db in room {}: {}", room_id, e.what());
        return {};
    }
}

bool
Cache::isRoomMember(const std::string &user_id, const std::string &room_id)
{
    try {
        auto txn = ro_txn(storage());
        auto db_ = getMembersDb(txn, room_id);

        std::string_view value;
        bool res = db_.get(txn, user_id, value);

        return res;
    } catch (std::exception &e) {
        nhlog::db()->warn(
          "Failed to read member membership ({}) in room ({}): {}", user_id, room_id, e.what());
    }
    return false;
}
