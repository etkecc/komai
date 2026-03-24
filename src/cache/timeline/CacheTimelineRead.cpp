// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cache/Cache.h"
#include "cache/core/Cache_p.h"

#include <algorithm>
#include <limits>
#include <unordered_map>

#include "emoji/EmojiNormalize.h"
#include "events/EventAccessors.h"
#include "settings/core/SettingsDefinitions.h"
#include <spdlog/logger.h>

#include "cache/api/CacheApiContext.h"
#include "cache/schema/RoomStore.h"
#include "cache/schema/RoomTimelineIndex.h"

constexpr size_t MAX_RESTORED_MESSAGES =
#if Q_PROCESSOR_WORDSIZE >= 5 // 40-bit or more, up to 2^(8*WORDSIZE) words addressable.
  30'000;
#elif Q_PROCESSOR_WORDSIZE == 4 // 32-bit address space limits mmaps
  5'000;
#else
#error Not enough virtual address space for the database on target CPU
#endif

void
MatrixStore::deleteOldMessages()
{
    auto txn      = beginTxn();
    auto room_ids = getRoomIds(txn);

    for (const auto &room_id : room_ids) {
        auto orderDb     = getEventOrderDb(txn, room_id);
        auto evToOrderDb = getEventToOrderDb(txn, room_id);
        auto o2m         = getOrderToMessageDb(txn, room_id);
        auto m2o         = getMessageToOrderDb(txn, room_id);
        auto eventsDb    = getEventsDb(txn, room_id);
        auto relationsDb = getRelationsDb(txn, room_id);

        uint64_t first, last;
        if (const auto lastEntry = room_timeline::lastOrderedIndex(
              txn, orderDb, cache::schema::RoomDb::EventOrder, room_id);
            lastEntry) {
            last = *lastEntry;
        } else {
            continue;
        }
        if (const auto firstEntry = room_timeline::firstOrderedIndex(
              txn, orderDb, cache::schema::RoomDb::EventOrder, room_id);
            firstEntry) {
            first = *firstEntry;
        } else {
            continue;
        }

        size_t message_count = static_cast<size_t>(last - first);
        if (message_count < MAX_RESTORED_MESSAGES)
            continue;

        const auto toDeleteCount = message_count - MAX_RESTORED_MESSAGES;
        room_timeline::trimOldestOrderEntriesWithReferences(
          txn, orderDb, eventsDb, relationsDb, evToOrderDb, m2o, o2m, room_id, toDeleteCount);
    }
    txn.commit();
}

void
MatrixStore::deleteOldData() noexcept
{
    try {
        deleteOldMessages();
    } catch (const db::Error &e) {
        cache::activeLoggers().db->error("failed to delete old messages: {}", e.what());
    }
}

std::optional<mtx::events::collections::TimelineEvents>
MatrixStore::getEvent(const std::string &room_id, std::string_view event_id)
{
    auto txn      = ro_txn(storage());
    auto eventsDb = getEventsDb(txn, room_id);

    try {
        return db::getJsonValue<mtx::events::collections::TimelineEvents>(
          txn, eventsDb, room_store::key(cache::schema::RoomDb::Events, room_id, event_id));
    } catch (std::exception &e) {
        cache::activeLoggers().db->error("Failed to parse message from cache {}", e.what());
        return std::nullopt;
    }
}

void
MatrixStore::storeEvent(const std::string &room_id,
                        const std::string &event_id,
                        const mtx::events::collections::TimelineEvents &event)
{
    auto txn        = beginTxn();
    auto eventsDb   = getEventsDb(txn, room_id);
    auto event_json = mtx::accessors::serialize_event(event);
    room_store::put(
      txn, eventsDb, cache::schema::RoomDb::Events, room_id, event_id, event_json.dump());
    txn.commit();
}

std::vector<std::string>
MatrixStore::relatedEvents(const std::string &room_id, const std::string &event_id)
{
    auto txn         = ro_txn(storage());
    auto relationsDb = getRelationsDb(txn, room_id);

    try {
        return db::listDupValues(
          txn, relationsDb, room_store::key(cache::schema::RoomDb::Related, room_id, event_id));
    } catch (const db::Error &e) {
        cache::activeLoggers().db->error("related events error: {}", e.what());
        return {};
    }
}

std::string
MatrixStore::getLastEventId(db::Transaction &txn, const std::string &room_id)
{
    db::Store orderDb;
    try {
        orderDb = getOrderToMessageDb(txn, room_id);
    } catch (const db::Error &e) {
        cache::activeLoggers().db->error(
          "Can't open db for room '{}', probably doesn't exist yet. ({})", room_id, e.what());
        return {};
    }

    return room_timeline::lastTimelineEventId(txn, orderDb, room_id).value_or("");
}

std::string
MatrixStore::getLastContentEventId(db::Transaction &txn, const std::string &room_id)
{
    constexpr uint64_t kScanLimit = 200;

    db::Store orderDb;
    db::Store eventsDb;
    try {
        orderDb  = getOrderToMessageDb(txn, room_id);
        eventsDb = getEventsDb(txn, room_id);
    } catch (const db::Error &e) {
        cache::activeLoggers().db->error(
          "Can't open db for room '{}', probably doesn't exist yet. ({})", room_id, e.what());
        return {};
    }

    const auto range = room_timeline::timelineRange(txn, orderDb, room_id);
    if (!range)
        return {};

    uint64_t scanned = 0;
    for (uint64_t idx = range->second;; --idx) {
        const auto eventId = room_timeline::timelineEventIdAtIndex(txn, orderDb, room_id, idx);
        if (!eventId)
            break;

        try {
            const auto event = db::getJsonValue<mtx::events::collections::TimelineEvents>(
              txn, eventsDb, room_store::key(cache::schema::RoomDb::Events, room_id, *eventId));
            if (event && mtx::accessors::is_message(*event))
                return *eventId;
        } catch (const std::exception &) {
            // Skip unparsable events.
        }

        if (idx == range->first || ++scanned >= kScanLimit)
            break;
    }

    return {};
}

std::optional<MatrixStore::TimelineRange>
MatrixStore::getTimelineRange(const std::string &room_id)
{
    auto txn = ro_txn(storage());
    db::Store orderDb;
    try {
        orderDb = getOrderToMessageDb(txn, room_id);
    } catch (const db::Error &e) {
        cache::activeLoggers().db->error(
          "Can't open db for room '{}', probably doesn't exist yet. ({})", room_id, e.what());
        return {};
    }

    const auto range = room_timeline::timelineRange(txn, orderDb, room_id);
    if (!range)
        return {};

    return TimelineRange{.first = range->first, .last = range->second};
}

std::optional<uint64_t>
MatrixStore::getTimelineIndex(const std::string &room_id, std::string_view event_id)
{
    if (room_id.empty() || event_id.empty())
        return {};

    auto txn = ro_txn(storage());

    db::Store orderDb;
    try {
        orderDb = getMessageToOrderDb(txn, room_id);
    } catch (const db::Error &e) {
        cache::activeLoggers().db->error(
          "Can't open db for room '{}', probably doesn't exist yet. ({})", room_id, e.what());
        return {};
    }

    return room_timeline::timelineIndexForEvent(txn, orderDb, room_id, event_id);
}

std::optional<uint64_t>
MatrixStore::getEventIndex(const std::string &room_id, std::string_view event_id)
{
    if (room_id.empty() || event_id.empty())
        return {};

    auto txn = ro_txn(storage());

    db::Store orderDb;
    try {
        orderDb = getEventToOrderDb(txn, room_id);
    } catch (const db::Error &e) {
        cache::activeLoggers().db->error(
          "Can't open db for room '{}', probably doesn't exist yet. ({})", room_id, e.what());
        return {};
    }

    return room_timeline::eventIndexForEvent(txn, orderDb, room_id, event_id);
}

std::optional<std::pair<uint64_t, std::string>>
MatrixStore::lastInvisibleEventAfter(const std::string &room_id, std::string_view event_id)
{
    if (room_id.empty() || event_id.empty())
        return {};

    auto txn = ro_txn(storage());

    db::Store orderDb;
    db::Store eventOrderDb;
    db::Store timelineDb;
    try {
        orderDb      = getEventToOrderDb(txn, room_id);
        eventOrderDb = getEventOrderDb(txn, room_id);
        timelineDb   = getMessageToOrderDb(txn, room_id);
    } catch (const db::Error &e) {
        cache::activeLoggers().db->error(
          "Can't open db for room '{}', probably doesn't exist yet. ({})", room_id, e.what());
        return {};
    }

    try {
        return room_timeline::lastInvisibleEventAfter(
          txn, orderDb, eventOrderDb, timelineDb, room_id, event_id);
    } catch (const db::Error &e) {
        cache::activeLoggers().db->error(
          "Failed to get last invisible event after {}", event_id, e.what());
        return {};
    }
}

std::optional<std::pair<uint64_t, std::string>>
MatrixStore::lastVisibleEvent(const std::string &room_id, std::string_view event_id)
{
    if (room_id.empty() || event_id.empty())
        return {};

    auto txn = ro_txn(storage());
    db::Store orderDb;
    db::Store eventOrderDb;
    db::Store timelineDb;
    try {
        orderDb      = getEventToOrderDb(txn, room_id);
        eventOrderDb = getEventOrderDb(txn, room_id);
        timelineDb   = getMessageToOrderDb(txn, room_id);

        return room_timeline::lastVisibleEvent(
          txn, orderDb, eventOrderDb, timelineDb, room_id, event_id);
    } catch (const db::Error &e) {
        cache::activeLoggers().db->error(
          "Failed to get last visible event after {}", event_id, e.what());
        return {};
    }
}

std::optional<std::string>
MatrixStore::getTimelineEventId(const std::string &room_id, uint64_t index)
{
    auto txn = ro_txn(storage());
    db::Store orderDb;
    try {
        orderDb = getOrderToMessageDb(txn, room_id);
    } catch (const db::Error &e) {
        cache::activeLoggers().db->error(
          "Can't open db for room '{}', probably doesn't exist yet. ({})", room_id, e.what());
        return {};
    }

    return room_timeline::timelineEventIdAtIndex(txn, orderDb, room_id, index);
}

std::vector<std::string>
MatrixStore::topUserReactions(const std::string &room_id, int lookbackDays, int maxResults)
{
    auto txn = ro_txn(storage());

    db::Store eventOrderDb;
    db::Store eventsDb;
    try {
        eventOrderDb = getEventOrderDb(txn, room_id);
        eventsDb     = getEventsDb(txn, room_id);
    } catch (const db::Error &e) {
        cache::activeLoggers().db->error(
          "Can't open db for room '{}', probably doesn't exist yet. ({})", room_id, e.what());
        return {};
    }

    const auto lastOrder = room_timeline::detail::lastOrderedEntry(
      txn, eventOrderDb, cache::schema::RoomDb::EventOrder, room_id);
    if (!lastOrder)
        return {};

    const auto localUser = localUserId_.toStdString();
    const auto cutoffMs =
      static_cast<uint64_t>(QDateTime::currentMSecsSinceEpoch() - lookbackDays * 86400000LL);

    std::unordered_map<std::string, int> frequency;
    uint64_t scanned   = 0;
    bool reachedCutoff = false;

    room_timeline::detail::forEachOrderedEntryFrom(
      txn,
      eventOrderDb,
      cache::schema::RoomDb::EventOrder,
      room_id,
      lastOrder->first,
      db::ScanDirection::Backward,
      [&](std::uint64_t /*index*/, std::string_view orderValue) {
          if (++scanned > settings::core::definitions::kMaxReactionScanEvents) {
              reachedCutoff = true;
              return false;
          }

          const auto entry = db::parseOrderEntry(orderValue);
          if (!entry.eventId)
              return true;

          try {
              const auto event = db::getJsonValue<mtx::events::collections::TimelineEvents>(
                txn,
                eventsDb,
                room_store::key(cache::schema::RoomDb::Events, room_id, *entry.eventId));
              if (!event)
                  return true;

              const auto ts = mtx::accessors::origin_server_ts_ms(*event);
              if (ts < cutoffMs) {
                  reachedCutoff = true;
                  return false;
              }

              if (auto *reaction =
                    std::get_if<mtx::events::RoomEvent<mtx::events::msg::Reaction>>(&*event);
                  reaction && reaction->sender == localUser &&
                  reaction->content.relations.annotates() &&
                  reaction->content.relations.annotates()->key) {
                  ++frequency[emoji::normalizeForComparison(
                    reaction->content.relations.annotates()->key.value())];
              }
          } catch (const std::exception &) {
              // Skip unparsable events.
          }

          return true;
      });

    // Sort by frequency descending, return top N keys.
    std::vector<std::pair<std::string, int>> sorted(frequency.begin(), frequency.end());
    std::sort(sorted.begin(), sorted.end(), [](const auto &a, const auto &b) {
        return a.second > b.second;
    });

    std::vector<std::string> result;
    result.reserve(static_cast<size_t>(maxResults));
    for (const auto &[key, count] : sorted) {
        result.push_back(key);
        if (static_cast<int>(result.size()) >= maxResults)
            break;
    }

    return result;
}
