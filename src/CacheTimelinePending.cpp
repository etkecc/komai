// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Cache.h"
#include "Cache_p.h"

#include "EventAccessors.h"
#include "Logging.h"
#include <nlohmann/json.hpp>
#include <QDateTime>

void
Cache::savePendingMessage(const std::string &room_id,
                         const mtx::events::collections::TimelineEvents &message)
{
    auto txn      = beginTxn();
    auto eventsDb = getEventsDb(txn, room_id);

    mtx::responses::Timeline timeline;
    timeline.events.push_back(message);
    saveTimelineMessages(txn, eventsDb, room_id, timeline);

    auto pending = getPendingMessagesDb(txn, room_id);

    int64_t now = QDateTime::currentMSecsSinceEpoch();
    pending.put(txn, db::toSv(now), mtx::accessors::event_id(message));

    txn.commit();
}

std::vector<std::string>
Cache::pendingEvents(const std::string &room_id)
{
    auto txn     = ro_txn(storage());
    auto pending = getPendingMessagesDb(txn, room_id);

    std::vector<std::string> pending_ids;

    try {
        db::forEachEntry(
          txn, pending, [&pending_ids](std::string_view /*ignored*/, std::string_view pendingTxn) {
              pending_ids.emplace_back(pendingTxn);
              return true;
          });
    } catch (const db::Error &e) {
        nhlog::db()->error("pending events error: {}", e.what());
    }

    return pending_ids;
}

std::optional<mtx::events::collections::TimelineEvents>
Cache::firstPendingMessage(const std::string &room_id)
{
    auto txn      = beginTxn();
    auto pending  = getPendingMessagesDb(txn, room_id);
    auto eventsDb = getEventsDb(txn, room_id);

    std::optional<mtx::events::collections::TimelineEvents> firstValid;
    std::vector<std::pair<std::string, std::string>> staleEntries;

    try {
        db::forEachEntry(
          txn,
          pending,
          [&eventsDb, &txn, &firstValid, &staleEntries](std::string_view timestamp,
                                                        std::string_view pendingTxn) {
              try {
                  if (auto event = db::getJsonValue<mtx::events::collections::TimelineEvents>(
                        txn, eventsDb, pendingTxn)) {
                      firstValid = std::move(*event);
                      return false;
                  }
              } catch (const nlohmann::json::exception &e) {
                  nhlog::db()->error("Failed to parse message from cache {}", e.what());
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
            pending.del(txn, timestamp, pendingTxn);
        txn.commit();
    }

    return firstValid;
}

void
Cache::removePendingStatus(const std::string &room_id, const std::string &txn_id)
{
    auto txn     = beginTxn();
    auto pending = getPendingMessagesDb(txn, room_id);

    db::removePendingEntriesByTxnId(txn, pending, txn_id);

    txn.commit();
}

void
Cache::clearTimeline(const std::string &room_id)
{
    auto txn         = beginTxn();
    auto eventsDb    = getEventsDb(txn, room_id);
    auto relationsDb = getRelationsDb(txn, room_id);

    auto orderDb     = getEventOrderDb(txn, room_id);
    auto evToOrderDb = getEventToOrderDb(txn, room_id);
    auto msg2orderDb = getMessageToOrderDb(txn, room_id);
    auto order2msgDb = getOrderToMessageDb(txn, room_id);

    db::cleanupTimelineBeforePrevBatchMarker(
      txn, orderDb, eventsDb, relationsDb, evToOrderDb, msg2orderDb, order2msgDb);

    txn.commit();
}

void
Cache::markSentNotification(const std::string &event_id)
{
    auto txn = beginTxn();
    db->notifications.put(txn, event_id, "");
    txn.commit();
}

void
Cache::removeReadNotification(const std::string &event_id)
{
    auto txn = beginTxn();

    db->notifications.del(txn, event_id);

    txn.commit();
}

bool
Cache::isNotificationSent(const std::string &event_id)
{
    auto txn = ro_txn(storage());

    std::string_view value;
    bool res = db->notifications.get(txn, event_id, value);

    return res;
}
