// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "Cache.h"
#include "Cache_p.h"

#include <limits>

#include "EventAccessors.h"
#include <spdlog/logger.h>

#include "CacheApiWrappers.h"

constexpr size_t MAX_RESTORED_MESSAGES =
#if Q_PROCESSOR_WORDSIZE >= 5 // 40-bit or more, up to 2^(8*WORDSIZE) words addressable.
  30'000;
#elif Q_PROCESSOR_WORDSIZE == 4 // 32-bit address space limits mmaps
  5'000;
#else
#error Not enough virtual address space for the database on target CPU
#endif

void
Cache::deleteOldMessages()
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
        if (const auto lastEntry = db::lastOrderedIndex(txn, orderDb); lastEntry) {
            last = *lastEntry;
        } else {
            continue;
        }
        if (const auto firstEntry = db::firstOrderedIndex(txn, orderDb); firstEntry) {
            first = *firstEntry;
        } else {
            continue;
        }

        size_t message_count = static_cast<size_t>(last - first);
        if (message_count < MAX_RESTORED_MESSAGES)
            continue;

        const auto toDeleteCount = message_count - MAX_RESTORED_MESSAGES;
        db::trimOldestOrderEntriesWithReferences(
          txn, orderDb, eventsDb, relationsDb, evToOrderDb, m2o, o2m, toDeleteCount);
    }
    txn.commit();
}

void
Cache::deleteOldData() noexcept
{
    try {
        deleteOldMessages();
    } catch (const db::Error &e) {
                    cache::activeLoggers().db->error("failed to delete old messages: {}", e.what());
    }
}

std::optional<mtx::events::collections::TimelineEvents>
Cache::getEvent(const std::string &room_id, std::string_view event_id)
{
    auto txn      = ro_txn(storage());
    auto eventsDb = getEventsDb(txn, room_id);

    try {
        return db::getJsonValue<mtx::events::collections::TimelineEvents>(txn, eventsDb, event_id);
    } catch (std::exception &e) {
                    cache::activeLoggers().db->error("Failed to parse message from cache {}", e.what());
        return std::nullopt;
    }
}

void
Cache::storeEvent(const std::string &room_id,
                  const std::string &event_id,
                  const mtx::events::collections::TimelineEvents &event)
{
    auto txn        = beginTxn();
    auto eventsDb   = getEventsDb(txn, room_id);
    auto event_json = mtx::accessors::serialize_event(event);
    eventsDb.put(txn, event_id, event_json.dump());
    txn.commit();
}

std::vector<std::string>
Cache::relatedEvents(const std::string &room_id, const std::string &event_id)
{
    auto txn         = ro_txn(storage());
    auto relationsDb = getRelationsDb(txn, room_id);

    try {
        return db::listDupValues(txn, relationsDb, event_id);
    } catch (const db::Error &e) {
                    cache::activeLoggers().db->error("related events error: {}", e.what());
        return {};
    }
}

std::string
Cache::getLastEventId(db::Transaction &txn, const std::string &room_id)
{
    db::Store orderDb;
    try {
        orderDb = getOrderToMessageDb(txn, room_id);
    } catch (const db::Error &e) {
                    cache::activeLoggers().db->error(
              "Can't open db for room '{}', probably doesn't exist yet. ({})", room_id, e.what());
        return {};
    }

    return db::lastTimelineEventId(txn, orderDb).value_or("");
}

std::optional<Cache::TimelineRange>
Cache::getTimelineRange(const std::string &room_id)
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

    const auto range = db::timelineRange(txn, orderDb);
    if (!range)
        return {};

    return TimelineRange{.first = range->first, .last = range->second};
}

std::optional<uint64_t>
Cache::getTimelineIndex(const std::string &room_id, std::string_view event_id)
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

    return db::timelineIndexForEvent(txn, orderDb, event_id);
}

std::optional<uint64_t>
Cache::getEventIndex(const std::string &room_id, std::string_view event_id)
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

    return db::eventIndexForEvent(txn, orderDb, event_id);
}

std::optional<std::pair<uint64_t, std::string>>
Cache::lastInvisibleEventAfter(const std::string &room_id, std::string_view event_id)
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
        return db::lastInvisibleEventAfter(txn, orderDb, eventOrderDb, timelineDb, event_id);
    } catch (const db::Error &e) {
                    cache::activeLoggers().db->error("Failed to get last invisible event after {}", event_id, e.what());
        return {};
    }
}

std::optional<std::pair<uint64_t, std::string>>
Cache::lastVisibleEvent(const std::string &room_id, std::string_view event_id)
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

        return db::lastVisibleEvent(txn, orderDb, eventOrderDb, timelineDb, event_id);
    } catch (const db::Error &e) {
                    cache::activeLoggers().db->error("Failed to get last visible event after {}", event_id, e.what());
        return {};
    }
}

std::optional<std::string>
Cache::getTimelineEventId(const std::string &room_id, uint64_t index)
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

    return db::timelineEventIdAtIndex(txn, orderDb, index);
}
