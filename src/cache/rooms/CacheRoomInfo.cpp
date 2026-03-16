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
#include "cache/schema/RoomStore.h"
#include "cache/schema/RoomTimelineIndex.h"

#include <nlohmann/json.hpp>

RoomInfo
MatrixStore::singleRoomInfo(const std::string &room_id)
{
    auto txn = ro_txn(storage());

    try {
        auto statesdb  = getStatesDb(txn, room_id);
        auto membersdb = getMembersDb(txn, room_id);
        if (auto info = cache::codec::getRoomInfo(txn, db->rooms, room_id)) {
            auto tmp = std::move(*info);
            tmp.member_count =
              room_store::countEntries(txn, membersdb, cache::schema::RoomDb::Members, room_id);
            tmp.join_rule    = getRoomJoinRule(txn, room_id, statesdb);
            tmp.guest_access = getRoomGuestAccess(txn, room_id, statesdb);
            tmp.avatar_url   = getRoomAvatarUrl(txn, room_id, statesdb, membersdb).toStdString();
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
MatrixStore::updateLastMessageTimestamp(const std::string &room_id, uint64_t ts)
{
    auto txn = beginTxn();

    try {
        if (auto info = cache::codec::getRoomInfo(txn, db->rooms, room_id)) {
            auto tmp                             = std::move(*info);
            tmp.approximate_last_modification_ts = ts;
            cache::codec::putRoomInfo(txn, db->rooms, room_id, tmp);
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
MatrixStore::getRoomInfo(const std::vector<std::string> &rooms)
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
                RoomInfo tmp   = cache::codec::parseRoomInfo(data);
                auto membersDb = getMembersDb(txn, room);
                tmp.member_count =
                  room_store::countEntries(txn, membersDb, cache::schema::RoomDb::Members, room);
                tmp.join_rule    = getRoomJoinRule(txn, room, statesdb);
                tmp.guest_access = getRoomGuestAccess(txn, room, statesdb);

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
                    RoomInfo tmp         = cache::codec::parseRoomInfo(data);
                    auto inviteMembersDb = getInviteMembersDb(txn, room);
                    tmp.member_count     = room_store::countEntries(
                      txn, inviteMembersDb, cache::schema::RoomDb::InviteMembers, room);

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
MatrixStore::roomAvatarUrl(const std::string &room_id)
{
    auto txn = ro_txn(storage());

    try {
        auto statesdb  = getStatesDb(txn, room_id);
        auto membersdb = getMembersDb(txn, room_id);
        return getRoomAvatarUrl(txn, room_id, statesdb, membersdb);
    } catch (const std::exception &e) {
        cache::activeLoggers().db->warn(
          "failed to get room avatar url for room '{}': {}", room_id, e.what());
    }

    return {};
}

std::vector<QString>
MatrixStore::roomIds()
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
MatrixStore::previousBatchToken(const std::string &room_id)
{
    auto txn = ro_txn(storage());
    try {
        auto orderDb = getEventOrderDb(txn, room_id);
        return room_timeline::firstPrevBatchToken(txn, orderDb, room_id).value_or("");
    } catch (...) {
        return "";
    }
}

QMap<QString, RoomInfo>
MatrixStore::roomInfo(bool withInvites)
{
    QMap<QString, RoomInfo> result;

    auto txn = ro_txn(storage());

    // Gather info about the joined rooms.
    db::forEachEntry(
      txn, db->rooms, [this, &txn, &result](std::string_view room_id, std::string_view room_data) {
          try {
              RoomInfo tmp   = cache::codec::parseRoomInfo(room_data);
              auto membersDb = getMembersDb(txn, std::string(room_id));
              tmp.member_count =
                room_store::countEntries(txn, membersDb, cache::schema::RoomDb::Members, room_id);
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
                  RoomInfo tmp         = cache::codec::parseRoomInfo(room_data);
                  auto inviteMembersDb = getInviteMembersDb(txn, std::string(room_id));
                  tmp.member_count     = room_store::countEntries(
                    txn, inviteMembersDb, cache::schema::RoomDb::InviteMembers, room_id);
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
MatrixStore::roomNamesAndAliases()
{
    auto txn = ro_txn(storage());

    std::vector<RoomNameAlias> result;
    result.reserve(db->rooms.size(txn));

    db::forEachEntry(
      txn, db->rooms, [this, &txn, &result](std::string_view room_id, std::string_view room_data) {
          try {
              RoomInfo info = cache::codec::parseRoomInfo(room_data);

              auto aliases =
                getStateEvent<mtx::events::state::CanonicalAlias>(txn, std::string(room_id));
              std::string alias;
              if (aliases) {
                  alias = aliases->content.alias;
              }

              const bool lowPrio =
                std::find(info.tags.begin(), info.tags.end(), "m.lowpriority") != info.tags.end();

              result.push_back(RoomNameAlias{
                .id              = std::string(room_id),
                .name            = std::move(info.name),
                .alias           = std::move(alias),
                .avatar_url      = std::move(info.avatar_url),
                .recent_activity = info.approximate_last_modification_ts,
                .is_tombstoned   = info.is_tombstoned,
                .is_space        = info.is_space,
                .is_low_priority = lowPrio,
              });
          } catch (std::exception &e) {
              cache::activeLoggers().db->warn(
                "Failed to add room {} to result: {}", room_id, e.what());
          }
          return true;
      });

    return result;
}
