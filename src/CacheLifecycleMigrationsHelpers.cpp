// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Cache.h"
#include "Cache_p.h"

#include <algorithm>
#include <functional>
#include <string_view>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

#include <mtx/responses/messages.hpp>

#include <spdlog/logger.h>

#include "CacheApiWrappers.h"
#include "db/Maintenance.h"

namespace cache::detail {

std::vector<std::pair<std::string, std::function<bool()>>>
buildPreMigrations(Cache *cache)
{
    return {
      {"2020.05.01",
       [cache]() {
           try {
               auto txn = cache->beginTxn(nullptr);
               auto pending_receipts =
                 db::openGlobalStore(cache->storage(), txn, db::catalog::GlobalDb::PendingReceipts);
               pending_receipts.drop(txn, true);
               txn.commit();
           } catch (const db::Error &) {
               cache::activeLoggers().db->critical(
                 "Failed to delete pending_receipts database in migration!");
               return false;
           }

           cache::activeLoggers().db->info("Successfully deleted pending receipts database.");
           return true;
       }},
      {"2020.07.05",
       [cache]() {
           try {
               auto txn      = cache->beginTxn(nullptr);
               auto room_ids = cache->getRoomIds(txn);

               for (const auto &room_id : room_ids) {
                   try {
                       auto messagesDb = db::openRoomStore(cache->storage(),
                                                           txn,
                                                           room_id,
                                                           db::catalog::RoomDb::LegacyMessages,
                                                           false);

                       // keep some old messages and batch token
                       {
                           mtx::responses::Timeline oldMessages;
                           db::forEachEntry(
                             txn,
                             messagesDb,
                             [&oldMessages](std::string_view /*ts*/,
                                            std::string_view stored_message) {
                                 auto j = nlohmann::json::parse(
                                   std::string_view(stored_message.data(), stored_message.size()));

                                 if (oldMessages.prev_batch.empty())
                                     oldMessages.prev_batch = j["token"].get<std::string>();
                                 else if (j["token"].get<std::string>() != oldMessages.prev_batch)
                                     return false;

                                 oldMessages.events.push_back(
                                   j["event"].get<mtx::events::collections::TimelineEvents>());
                                 return true;
                             });
                           // messages were stored in reverse order, so we
                           // need to reverse them
                           std::reverse(oldMessages.events.begin(), oldMessages.events.end());
                           // save messages using the new method
                           auto eventsDb = cache->getEventsDb(txn, room_id);
                           cache->saveTimelineMessages(txn, eventsDb, room_id, oldMessages);
                       }

                       // delete old messages db
                       messagesDb.drop(txn, true);
                   } catch (std::exception &e) {
                       cache::activeLoggers().db->error(
                         "While migrating messages from {}, ignoring error {}", room_id, e.what());
                   }
               }
               txn.commit();
           } catch (const db::Error &) {
               cache::activeLoggers().db->critical(
                 "Failed to delete messages database in migration!");
               return false;
           }

           cache::activeLoggers().db->info("Successfully deleted pending receipts database.");
           return true;
       }},
      {"2020.10.20",
       [cache]() {
           try {
               auto txn = cache->beginTxn();
               db::maintenance::migrateLegacyOlmShardsV1ToV2(cache->storage(), txn);
               txn.commit();
           } catch (const db::Error &) {
               cache::activeLoggers().db->critical("Failed to migrate olm sessions,");
               return false;
           }

           cache::activeLoggers().db->info("Successfully migrated olm sessions.");
           return true;
       }},
    };
}

std::vector<std::pair<std::string, std::function<bool()>>>
buildPostMigrations(Cache *cache)
{
    return {
      {"2022.04.08",
       [cache]() {
           auto txn = cache->beginTxn(nullptr);
           std::string error;
           if (!db::maintenance::migrateLegacyMegolmSessionIndexes(cache->storage(), txn, &error)) {
               cache::activeLoggers().db->warn(
                 "Failed to migrate stored megolm session to have no sender key: {}", error);
               return false;
           }

           txn.commit();
           return true;
       }},
      {"2023.03.12",
       [cache]() {
           try {
               auto txn      = cache->beginTxn(nullptr);
               auto room_ids = cache->getRoomIds(txn);

               for (const auto &room_id : room_ids) {
                   std::string error;
                   if (!db::maintenance::migrateLegacyStateByKeyToStatesKey(
                         cache->storage(), txn, room_id, &error)) {
                       cache::activeLoggers().db->error(
                         "While migrating state events from {}, ignoring error {}", room_id, error);
                   }
               }
               txn.commit();
           } catch (const db::Error &) {
               cache::activeLoggers().db->critical(
                 "Failed to convert states key database in migration!");
               return false;
           }

           cache::activeLoggers().db->info("Successfully updated states key database format.");
           return true;
       }},
      {"2023.10.22",
       [cache]() {
           // migrate olm sessions to a single db
           try {
               auto txn = cache->beginTxn(nullptr);
               if (db::maintenance::migrateLegacyOlmShardsV2ToUnified(
                     cache->storage(), txn, cache->db->olmSessions))
                   txn.commit();
           } catch (const db::Error &e) {
               cache::activeLoggers().db->critical(
                 "Failed to convert olm sessions database in migration! {}", e.what());
               return false;
           }

           cache::activeLoggers().db->info("Successfully updated olm sessions database format.");
           return true;
       }},
    };
}

} // namespace cache::detail
