// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "db/Schema.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <exception>
#include <stdexcept>
#include <map>
#include <vector>

#include <nlohmann/json.hpp>

#include "db/Backend.h"
#include "db/Catalog.h"
#include "db/DbTypes.h"
#include "db/OlmSessionIndex.h"
#include "db/Open.h"
#include "db/Scan.h"
#include "db/StateIndex.h"
#include "db/Json.h"

namespace {

constexpr std::array<db::catalog::RoomDb, 12> kRoomDbsForFullResync = {
  db::catalog::RoomDb::State,
  db::catalog::RoomDb::LegacyStateByKey,
  db::catalog::RoomDb::AccountData,
  db::catalog::RoomDb::Members,
  db::catalog::RoomDb::LegacyMentions,
  db::catalog::RoomDb::Events,
  db::catalog::RoomDb::EventOrder,
  db::catalog::RoomDb::EventToOrder,
  db::catalog::RoomDb::MessageToOrder,
  db::catalog::RoomDb::OrderToMessage,
  db::catalog::RoomDb::Pending,
  db::catalog::RoomDb::Related,
};

} // namespace

namespace db {

std::span<const catalog::RoomDb>
roomDbsForFullResync() noexcept
{
    return std::span<const catalog::RoomDb>(kRoomDbsForFullResync.data(),
                                            kRoomDbsForFullResync.size());
}

bool
tryDropNamedDbi(Backend &backend, Txn &txn, std::string_view dbName, std::string *error) noexcept
{
    if (error)
        error->clear();

    try {
        openNamedDbi(backend, txn, dbName, false).drop(txn, true);
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

bool
migrateLegacyStateByKeyToStatesKey(Backend &backend,
                                   Txn &txn,
                                   std::string_view roomId,
                                   std::string *error) noexcept
{
    if (error)
        error->clear();

    try {
        auto oldStateskeyDb = openRoomDbi(backend, txn, roomId, catalog::RoomDb::LegacyStateByKey);
        auto newStateskeyDb = openRoomDbi(backend, txn, roomId, catalog::RoomDb::StatesKey);

        forEachEntry(
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

bool
migrateLegacyMegolmSessionIndexes(Backend &backend, Txn &txn, std::string *error) noexcept
{
    if (error)
        error->clear();

    try {
        auto inboundMegolmSessionDb =
          openGlobalDbi(backend, txn, catalog::GlobalDb::InboundMegolmSessions);
        auto outboundMegolmSessionDb =
          openGlobalDbi(backend, txn, catalog::GlobalDb::OutboundMegolmSessions);
        auto megolmSessionDataDb =
          openGlobalDbi(backend, txn, catalog::GlobalDb::MegolmSessionsData);

        try {
            outboundMegolmSessionDb.drop(txn, false);
        } catch (...) {
        }

        std::map<std::string, std::string> inboundSessions;
        std::map<std::string, std::string> megolmSessionData;
        forEachEntry(txn,
                     inboundMegolmSessionDb,
                     [&txn, &megolmSessionDataDb, &inboundSessions, &megolmSessionData](
                       std::string_view key, std::string_view value) {
                         nlohmann::json indexVal;
                         if (!db::parseJsonValue(key, indexVal))
                             throw std::runtime_error("invalid legacy megolm index key");
                         if (!indexVal.contains("sender_key") ||
                             !indexVal.at("sender_key").is_string())
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

void
migrateLegacyOlmShardsV1ToV2(Backend &backend, Txn &txn)
{
    const auto dbNames = backend.listDbiNames(txn);
    for (const auto &dbName : dbNames) {
        if (!catalog::isLegacyOlmShardV1(dbName))
            continue;

        auto oldDb = openNamedDbi(backend, txn, dbName, false);
        std::vector<std::pair<std::string, std::string>> sessions;

        forEachEntry(
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

        auto newDb = openNamedDbi(backend, txn, catalog::legacyOlmShardV2NameFromV1(dbName), true);
        for (const auto &[sessionKey, pickled] : sessions) {
            nlohmann::json value;
            value["ts"] = 0;
            value["s"]  = pickled;
            newDb.put(txn, sessionKey, value.dump());
        }
    }
}

bool
migrateLegacyOlmShardsV2ToUnified(Backend &backend, Txn &txn, Dbi &olmSessions)
{
    const auto dbNames = backend.listDbiNames(txn);
    bool migrated      = false;

    for (const auto &dbName : dbNames) {
        if (!catalog::isLegacyOlmShardV2(dbName))
            continue;

        migrated      = true;
        auto curveKey = *catalog::legacyOlmCurveFromV2Name(dbName);
        auto oldDb    = openNamedDbi(backend, txn, dbName, false);
        forEachEntry(
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

} // namespace db
