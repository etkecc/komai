// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cache/Cache.h"
#include "cache/core/Cache_p.h"

#include <algorithm>
#include <limits>

#include <nlohmann/json.hpp>
#include <spdlog/logger.h>

#include <QHash>

#include "cache/api/CacheApiWrappers.h"
#include "db/MemberInfo.h"

QHash<QString, RoomInfo>
Cache::invites()
{
    QHash<QString, RoomInfo> result;

    auto txn = ro_txn(storage());
    db::forEachEntry(
      txn,
      db->invites,
      [this, &txn, &result](std::string_view room_id, std::string_view room_data) {
          try {
              RoomInfo tmp     = db::parseRoomInfo(room_data);
              tmp.member_count = getInviteMembersDb(txn, std::string(room_id)).size(txn);
              result.insert(QString::fromStdString(std::string(room_id)), std::move(tmp));
          } catch (const std::exception &e) {
              cache::activeLoggers().db->warn("failed to parse room info for invite: "
                                              "room_id ({}), {}: {}",
                                              room_id,
                                              std::string(room_data),
                                              e.what());
          }
          return true;
      });

    return result;
}

std::optional<RoomInfo>
Cache::invite(std::string_view roomid)
{
    std::optional<RoomInfo> result;

    auto txn = ro_txn(storage());

    std::string_view room_data;

    if (db->invites.get(txn, roomid, room_data)) {
        try {
            RoomInfo tmp     = db::parseRoomInfo(room_data);
            tmp.member_count = getInviteMembersDb(txn, std::string(roomid)).size(txn);
            result           = std::move(tmp);
        } catch (const std::exception &e) {
            cache::activeLoggers().db->warn("failed to parse room info for invite: "
                                            "room_id ({}), {}: {}",
                                            roomid,
                                            std::string(room_data),
                                            e.what());
        }
    }

    return result;
}

std::optional<MemberInfo>
Cache::getInviteMember(const std::string &room_id, const std::string &user_id)
{
    if (user_id.empty() || !db::isOpen(storage()))
        return std::nullopt;

    try {
        auto txn = ro_txn(storage());

        auto membersdb = getInviteMembersDb(txn, room_id);

        return db::getMemberInfo(txn, membersdb, user_id);
    } catch (std::exception &e) {
        cache::activeLoggers().db->warn(
          "Failed to read member ({}) in invite room ({}): {}", user_id, room_id, e.what());
    }
    return std::nullopt;
}

std::vector<RoomMember>
Cache::getMembersFromInvite(const std::string &room_id, std::size_t startIndex, std::size_t len)
{
    try {
        auto txn = ro_txn(storage());
        std::vector<RoomMember> members;

        auto db_ = getInviteMembersDb(txn, room_id);
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
                                   tmp.is_direct,
                                 });
                             } catch (const nlohmann::json::exception &e) {
                                 cache::activeLoggers().db->warn("{}", e.what());
                             }
                             return true;
                         });

        return members;
    } catch (const db::Error &e) {
        cache::activeLoggers().db->error(
          "Failed to retrieve members from db in room {}: {}", room_id, e.what());
        return {};
    }
}
