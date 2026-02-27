// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-License-Identifier: GPL-3.0-or-later
//
// SPDX-FileCopyrightText: Komai Contributors

#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <map>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "cache/schema/CacheSchema.h"
#include "db/Json.h"
#include "db/storage/Core.h"
#include "db/storage/Crypto.h"
#include "db/storage/Scan.h"
#include "db/storage/State.h"

namespace cache::migrations {

using cache::schema::GlobalDb;
using cache::schema::RoomDb;

inline std::span<const RoomDb>
fullResyncRoomDbs() noexcept
{
    static constexpr std::array<cache::schema::RoomDb, 12> kRoomDbsForFullResync = {
      RoomDb::State,
      RoomDb::LegacyStateByKey,
      RoomDb::AccountData,
      RoomDb::Members,
      RoomDb::LegacyMentions,
      RoomDb::Events,
      RoomDb::EventOrder,
      RoomDb::EventToOrder,
      RoomDb::MessageToOrder,
      RoomDb::OrderToMessage,
      RoomDb::Pending,
      RoomDb::Related,
    };

    return {kRoomDbsForFullResync.data(), kRoomDbsForFullResync.size()};
}

inline bool
tryDropNamedStore(db::Database &database,
                  db::Transaction &txn,
                  std::string_view dbName,
                  std::string *error = nullptr) noexcept
{
    try {
        db::openNamedStore(database, txn, dbName, false).drop(txn, true);
        return true;
    } catch (const std::exception &e) {
        if (error)
            *error = e.what();
        return false;
    } catch (...) {
        if (error)
            *error = "unknown error";
        return false;
    }
}

inline bool
migrateLegacyStateByKeyToStatesKey(db::Database &database,
                                   db::Transaction &txn,
                                   std::string_view roomId,
                                   std::string *error) noexcept
{
    if (error)
        error->clear();

    try {
        auto oldStateskeyDb =
          cache::schema::openRoomStore(database, txn, roomId, RoomDb::LegacyStateByKey, true);
        auto newStateskeyDb =
          cache::schema::openRoomStore(database, txn, roomId, RoomDb::StatesKey, true);

        db::forEachEntry(
          txn,
          oldStateskeyDb,
          [&txn, &newStateskeyDb](std::string_view eventType, std::string_view data) {
              nlohmann::json parsed;
              if (!db::parseJsonValue(data, parsed))
                  throw std::runtime_error("invalid legacy state-by-key payload");
              putStateEventId(
                txn, newStateskeyDb, eventType, parsed.value("key", ""), parsed.value("id", ""));
              return true;
          });

        oldStateskeyDb.drop(txn, true);
        return true;
    } catch (const std::exception &e) {
        if (error)
            *error = e.what();
        return false;
    } catch (...) {
        if (error)
            *error = "unknown error";
        return false;
    }
}

inline bool
migrateLegacyMegolmSessionIndexes(db::Database &database,
                                  db::Transaction &txn,
                                  std::string *error) noexcept
{
    if (error)
        error->clear();

    try {
        auto inboundMegolmSessionDb =
          cache::schema::openGlobalStore(database, txn, GlobalDb::InboundMegolmSessions, true);
        auto outboundMegolmSessionDb =
          cache::schema::openGlobalStore(database, txn, GlobalDb::OutboundMegolmSessions, true);
        auto megolmSessionDataDb =
          cache::schema::openGlobalStore(database, txn, GlobalDb::MegolmSessionsData, true);

        try {
            outboundMegolmSessionDb.drop(txn, false);
        } catch (...) {
        }

        std::map<std::string, std::string> inboundSessions;
        std::map<std::string, std::string> megolmSessionData;
        db::forEachEntry(
          txn,
          inboundMegolmSessionDb,
          [&txn, &megolmSessionDataDb, &inboundSessions, &megolmSessionData](
            std::string_view key, std::string_view value) {
              nlohmann::json indexVal;
              if (!db::parseJsonValue(key, indexVal))
                  throw std::runtime_error("invalid legacy megolm index key");
              if (!indexVal.contains("sender_key") || !indexVal.at("sender_key").is_string())
                  return true;
              auto senderKey = indexVal["sender_key"].get<std::string>();
              indexVal.erase("sender_key");

              std::string_view dataVal;
              if (megolmSessionDataDb.get(txn, key, dataVal)) {
                  nlohmann::json data;
                  if (!db::parseJsonValue(dataVal, data))
                      throw std::runtime_error("invalid legacy megolm metadata payload");
                  data["sender_key"] = senderKey;

                  const auto newKey         = indexVal.dump();
                  inboundSessions[newKey]   = std::string(value);
                  megolmSessionData[newKey] = data.dump();
              }
              return true;
          });

        inboundMegolmSessionDb.drop(txn, false);
        megolmSessionDataDb.drop(txn, false);

        for (const auto &[migratedKey, migratedValue] : inboundSessions)
            inboundMegolmSessionDb.put(txn, migratedKey, migratedValue);

        for (const auto &[migratedKey, migratedValue] : megolmSessionData)
            megolmSessionDataDb.put(txn, migratedKey, migratedValue);

        return true;
    } catch (const std::exception &e) {
        if (error)
            *error = e.what();
        return false;
    } catch (...) {
        if (error)
            *error = "unknown error";
        return false;
    }
}

inline void
migrateLegacyOlmShardsV1ToV2(db::Database &database, db::Transaction &txn)
{
    const auto dbNames = database.listStoreNames(txn);
    for (const auto &dbName : dbNames) {
        if (!db::catalog::isLegacyOlmShardV1(dbName))
            continue;

        auto oldDb = db::openNamedStore(database, txn, dbName, false);
        std::vector<std::pair<std::string, std::string>> sessions;

        db::forEachEntry(
          txn, oldDb, [&sessions](std::string_view sessionId, std::string_view sessionValue) {
              const bool invalid =
                std::any_of(sessionValue.begin(), sessionValue.end(), [](char c) {
                    return !std::isprint(static_cast<unsigned char>(c));
                });
              if (!invalid)
                  sessions.emplace_back(sessionId, sessionValue);
              return true;
          });

        oldDb.drop(txn, true);

        auto newDb =
          db::openNamedStore(database, txn, db::catalog::legacyOlmShardV2NameFromV1(dbName), true);
        for (const auto &[sessionKey, pickled] : sessions) {
            nlohmann::json value;
            value["ts"] = 0;
            value["s"]  = pickled;
            newDb.put(txn, sessionKey, value.dump());
        }
    }
}

inline bool
migrateLegacyOlmShardsV2ToUnified(db::Database &database,
                                  db::Transaction &txn,
                                  db::Store &olmSessions)
{
    const auto dbNames = database.listStoreNames(txn);
    bool migrated      = false;

    for (const auto &dbName : dbNames) {
        if (!db::catalog::isLegacyOlmShardV2(dbName))
            continue;

        migrated      = true;
        auto curveKey = *db::catalog::legacyOlmCurveFromV2Name(dbName);
        auto oldDb    = db::openNamedStore(database, txn, dbName, false);
        db::forEachEntry(
          txn,
          oldDb,
          [&txn, &olmSessions, &curveKey](std::string_view sessionId, std::string_view value) {
              putOlmSessionValue(txn, olmSessions, curveKey, sessionId, value);
              return true;
          });

        oldDb.drop(txn, true);
    }

    return migrated;
}

} // namespace cache::migrations
