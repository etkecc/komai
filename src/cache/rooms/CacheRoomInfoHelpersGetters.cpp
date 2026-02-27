// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cache/Cache.h"
#include "cache/core/Cache_p.h"

#include <map>
#include <string_view>
#include <vector>

#include <QMap>
#include <spdlog/logger.h>

#include "cache/api/CacheApiContext.h"
#include "db/RoomInfo.h"

#include <nlohmann/json.hpp>

RoomInfo
Cache::singleRoomInfo(const std::string &room_id)
{
    auto txn = ro_txn(storage());

    try {
        auto statesdb = getStatesDb(txn, room_id);
        if (auto info = db::getRoomInfo(txn, db->rooms, room_id)) {
            auto tmp         = std::move(*info);
            tmp.member_count = getMembersDb(txn, room_id).size(txn);
            tmp.join_rule    = getRoomJoinRule(txn, statesdb);
            tmp.guest_access = getRoomGuestAccess(txn, statesdb);
            return tmp;
        }
    } catch (const db::Error &e) {
        cache::activeLoggers().db->warn(
          "failed to read room info from db: room_id ({}), {}", room_id, e.what());
    } catch (const std::exception &e) {
        cache::activeLoggers().db->warn(
          "failed to parse room info for room '{}': {}", room_id, e.what());
    }

    return RoomInfo();
}

void
Cache::updateLastMessageTimestamp(const std::string &room_id, uint64_t ts)
{
    auto txn = beginTxn();

    try {
        if (auto info = db::getRoomInfo(txn, db->rooms, room_id)) {
            auto tmp                             = std::move(*info);
            tmp.approximate_last_modification_ts = ts;
            db::putRoomInfo(txn, db->rooms, room_id, tmp);
            txn.commit();
            return;
        }
    } catch (const db::Error &e) {
        cache::activeLoggers().db->warn(
          "failed to read room info from db: room_id ({}), {}", room_id, e.what());
    } catch (const std::exception &e) {
        cache::activeLoggers().db->warn(
          "failed to parse room info for room '{}': {}", room_id, e.what());
    }
}

std::map<QString, RoomInfo>
Cache::getRoomInfo(const std::vector<std::string> &rooms)
{
    std::map<QString, RoomInfo> room_info;

    // TODO This should be read only.
    auto txn = beginTxn();

    for (const auto &room : rooms) {
        std::string_view data;
        auto statesdb = getStatesDb(txn, room);

        // Check if the room is joined.
        if (db->rooms.get(txn, room, data)) {
            try {
                RoomInfo tmp     = db::parseRoomInfo(data);
                tmp.member_count = getMembersDb(txn, room).size(txn);
                tmp.join_rule    = getRoomJoinRule(txn, statesdb);
                tmp.guest_access = getRoomGuestAccess(txn, statesdb);

                room_info.emplace(QString::fromStdString(room), std::move(tmp));
            } catch (const std::exception &e) {
                cache::activeLoggers().db->warn("failed to parse room info: room_id ({}), {}: {}",
                                                room,
                                                std::string(data.data(), data.size()),
                                                e.what());
            }
        } else {
            // Check if the room is an invite.
            if (db->invites.get(txn, room, data)) {
                try {
                    RoomInfo tmp     = db::parseRoomInfo(data);
                    tmp.member_count = getInviteMembersDb(txn, room).size(txn);

                    room_info.emplace(QString::fromStdString(room), std::move(tmp));
                } catch (const std::exception &e) {
                    cache::activeLoggers().db->warn("failed to parse room info for invite: "
                                                    "room_id ({}), {}: {}",
                                                    room,
                                                    std::string(data.data(), data.size()),
                                                    e.what());
                }
            }
        }
    }

    txn.commit();

    return room_info;
}

QString
Cache::roomAvatarUrl(const std::string &room_id)
{
    auto txn = ro_txn(storage());

    try {
        auto statesdb  = getStatesDb(txn, room_id);
        auto membersdb = getMembersDb(txn, room_id);
        return getRoomAvatarUrl(txn, statesdb, membersdb);
    } catch (const std::exception &e) {
        cache::activeLoggers().db->warn(
          "failed to get room avatar url for room '{}': {}", room_id, e.what());
    }

    return {};
}

std::vector<QString>
Cache::roomIds()
{
    auto txn = ro_txn(storage());

    std::vector<QString> rooms;
    rooms.reserve(db->rooms.size(txn));
    db::forEachUniqueKey(txn, db->rooms, [&rooms](std::string_view room_id) {
        rooms.push_back(QString::fromStdString(std::string(room_id)));
        return true;
    });

    return rooms;
}

std::string
Cache::previousBatchToken(const std::string &room_id)
{
    auto txn = ro_txn(storage());
    try {
        auto orderDb = getEventOrderDb(txn, room_id);
        return db::firstPrevBatchToken(txn, orderDb).value_or("");
    } catch (...) {
        return "";
    }
}

QMap<QString, RoomInfo>
Cache::roomInfo(bool withInvites)
{
    QMap<QString, RoomInfo> result;

    auto txn = ro_txn(storage());

    // Gather info about the joined rooms.
    db::forEachEntry(
      txn, db->rooms, [this, &txn, &result](std::string_view room_id, std::string_view room_data) {
          try {
              RoomInfo tmp     = db::parseRoomInfo(room_data);
              tmp.member_count = getMembersDb(txn, std::string(room_id)).size(txn);
              result.insert(QString::fromStdString(std::string(room_id)), std::move(tmp));
          } catch (const std::exception &e) {
              cache::activeLoggers().db->warn(
                "failed to parse room info for joined room ({}): {}", room_id, e.what());
          }
          return true;
      });

    if (withInvites) {
        // Gather info about the invites.
        db::forEachEntry(
          txn,
          db->invites,
          [this, &txn, &result](std::string_view room_id, std::string_view room_data) {
              try {
                  RoomInfo tmp     = db::parseRoomInfo(room_data);
                  tmp.member_count = getInviteMembersDb(txn, std::string(room_id)).size(txn);
                  result.insert(QString::fromStdString(std::string(room_id)), std::move(tmp));
              } catch (const std::exception &e) {
                  cache::activeLoggers().db->warn(
                    "failed to parse room info for invite room ({}): {}", room_id, e.what());
              }
              return true;
          });
    }

    return result;
}

std::vector<RoomNameAlias>
Cache::roomNamesAndAliases()
{
    auto txn = ro_txn(storage());

    std::vector<RoomNameAlias> result;
    result.reserve(db->rooms.size(txn));

    db::forEachEntry(
      txn, db->rooms, [this, &txn, &result](std::string_view room_id, std::string_view room_data) {
          try {
              RoomInfo info = db::parseRoomInfo(room_data);

              auto aliases =
                getStateEvent<mtx::events::state::CanonicalAlias>(txn, std::string(room_id));
              std::string alias;
              if (aliases) {
                  alias = aliases->content.alias;
              }

              result.push_back(RoomNameAlias{
                .id              = std::string(room_id),
                .name            = std::move(info.name),
                .alias           = std::move(alias),
                .recent_activity = info.approximate_last_modification_ts,
                .is_tombstoned   = info.is_tombstoned,
                .is_space        = info.is_space,
              });
          } catch (std::exception &e) {
              cache::activeLoggers().db->warn(
                "Failed to add room {} to result: {}", room_id, e.what());
          }
          return true;
      });

    return result;
}
