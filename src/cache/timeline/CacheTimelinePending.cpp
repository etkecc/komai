// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cache/Cache.h"
#include "cache/core/Cache_p.h"

#include "events/EventAccessors.h"
#include <QDateTime>
#include <nlohmann/json.hpp>

#include <spdlog/logger.h>

#include "cache/api/CacheApiContext.h"
#include "cache/schema/RoomStore.h"
#include "cache/schema/RoomTimelineIndex.h"

void
MatrixStore::savePendingMessage(const std::string &room_id,
                                const mtx::events::collections::TimelineEvents &message)
{
    auto txn      = beginTxn();
    auto eventsDb = getEventsDb(txn, room_id);

    mtx::responses::Timeline timeline;
    timeline.events.push_back(message);
    saveTimelineMessages(txn, eventsDb, room_id, timeline);

    auto pending = getPendingMessagesDb(txn, room_id);

    int64_t now = QDateTime::currentMSecsSinceEpoch();
    pending.put(txn,
                room_store::orderedIndexKey(
                  cache::schema::RoomDb::Pending, room_id, static_cast<std::uint64_t>(now)),
                mtx::accessors::event_id(message));

    txn.commit();
}

std::vector<std::string>
MatrixStore::pendingEvents(const std::string &room_id)
{
    auto txn     = ro_txn(storage());
    auto pending = getPendingMessagesDb(txn, room_id);

    std::vector<std::string> pending_ids;

    try {
        room_store::forEachEntry(txn,
                                 pending,
                                 cache::schema::RoomDb::Pending,
                                 room_id,
                                 [&pending_ids](std::string_view, std::string_view pendingTxn) {
                                     pending_ids.emplace_back(pendingTxn);
                                     return true;
                                 });
    } catch (const db::Error &e) {
        cache::activeLoggers().db->error("pending events error: {}", e.what());
    }

    return pending_ids;
}

std::optional<mtx::events::collections::TimelineEvents>
MatrixStore::firstPendingMessage(const std::string &room_id)
{
    auto txn      = beginTxn();
    auto pending  = getPendingMessagesDb(txn, room_id);
    auto eventsDb = getEventsDb(txn, room_id);

    std::optional<mtx::events::collections::TimelineEvents> firstValid;
    std::vector<std::pair<std::string, std::string>> staleEntries;

    try {
        room_store::forEachEntry(
          txn,
          pending,
          cache::schema::RoomDb::Pending,
          room_id,
          [&eventsDb, &txn, &room_id, &firstValid, &staleEntries](std::string_view timestamp,
                                                                  std::string_view pendingTxn) {
              try {
                  if (auto event = db::getJsonValue<mtx::events::collections::TimelineEvents>(
                        txn,
                        eventsDb,
                        room_store::key(cache::schema::RoomDb::Events, room_id, pendingTxn))) {
                      firstValid = std::move(*event);
                      return false;
                  }
              } catch (const nlohmann::json::exception &e) {
                  cache::activeLoggers().db->error("Failed to parse message from cache {}",
                                                   e.what());
                  staleEntries.emplace_back(std::string(timestamp), std::string(pendingTxn));
                  return true;
              }

              staleEntries.emplace_back(std::string(timestamp), std::string(pendingTxn));
              return true;
          });
    } catch (const db::Error &e) {
    }

    if (!staleEntries.empty()) {
        for (const auto &[timestamp, pendingTxn] : staleEntries)
            room_store::del(
              txn, pending, cache::schema::RoomDb::Pending, room_id, timestamp, pendingTxn);
        txn.commit();
    }

    return firstValid;
}

void
MatrixStore::removePendingStatus(const std::string &room_id, const std::string &txn_id)
{
    auto txn     = beginTxn();
    auto pending = getPendingMessagesDb(txn, room_id);

    room_timeline::removePendingEntriesByTxnId(txn, pending, room_id, txn_id);

    txn.commit();
}

void
MatrixStore::clearTimeline(const std::string &room_id)
{
    auto txn         = beginTxn();
    auto eventsDb    = getEventsDb(txn, room_id);
    auto relationsDb = getRelationsDb(txn, room_id);

    auto orderDb     = getEventOrderDb(txn, room_id);
    auto evToOrderDb = getEventToOrderDb(txn, room_id);
    auto msg2orderDb = getMessageToOrderDb(txn, room_id);
    auto order2msgDb = getOrderToMessageDb(txn, room_id);

    room_timeline::cleanupTimelineBeforePrevBatchMarker(
      txn, orderDb, eventsDb, relationsDb, evToOrderDb, msg2orderDb, order2msgDb, room_id);

    txn.commit();
}

void
MatrixStore::markSentNotification(const std::string &event_id)
{
    auto txn = beginTxn();
    db->notifications.put(txn, event_id, "");
    txn.commit();
}

void
MatrixStore::removeReadNotification(const std::string &event_id)
{
    auto txn = beginTxn();

    db->notifications.del(txn, event_id);

    txn.commit();
}

bool
MatrixStore::isNotificationSent(const std::string &event_id)
{
    auto txn = ro_txn(storage());

    std::string_view value;
    bool res = db->notifications.get(txn, event_id, value);

    return res;
}
