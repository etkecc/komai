// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

#include "cache/schema/RoomStore.h"
#include "db/DbTypes.h"
#include "db/OrderEntry.h"

namespace cache::room_timeline {

std::optional<std::uint64_t>
eventIndexForEvent(db::Transaction &txn,
                   db::Store &eventToOrderDb,
                   std::string_view roomId,
                   std::string_view eventId);

void
removeTimelineEventReferences(db::Transaction &txn,
                              db::Store &eventsDb,
                              db::Store &relationsDb,
                              db::Store &eventToOrderDb,
                              db::Store &messageToOrderDb,
                              db::Store &orderToMessageDb,
                              std::string_view roomId,
                              std::string_view eventId);

namespace detail {

inline db::PutFlags
sanitizeOrderedPutFlags(db::PutFlags flags)
{
    return flags == db::PutFlags::Append ? db::PutFlags::None : flags;
}

inline std::string
orderedUpperBound(cache::schema::RoomDb db, std::string_view roomId)
{
    std::string key = room_store::prefix(db, roomId);
    key.append(sizeof(std::uint64_t), static_cast<char>(0xff));
    return key;
}

inline std::optional<std::pair<std::uint64_t, std::string>>
firstOrderedEntry(db::Transaction &txn,
                  db::Store &store,
                  cache::schema::RoomDb db,
                  std::string_view roomId)
{
    auto cursor = db::Cursor::open(txn, store);

    const auto keyPrefix = room_store::prefix(db, roomId);
    std::string key;
    std::string value;
    if (!cursor.moveToRange(keyPrefix, key, value) || !key.starts_with(keyPrefix))
        return std::nullopt;

    const auto index = room_store::orderedIndexFromKey(db, roomId, key);
    if (!index)
        return std::nullopt;

    return std::pair<std::uint64_t, std::string>{*index, std::move(value)};
}

inline std::optional<std::pair<std::uint64_t, std::string>>
lastOrderedEntry(db::Transaction &txn,
                 db::Store &store,
                 cache::schema::RoomDb db,
                 std::string_view roomId)
{
    auto cursor = db::Cursor::open(txn, store);

    const auto keyPrefix  = room_store::prefix(db, roomId);
    const auto upperBound = orderedUpperBound(db, roomId);

    std::string key;
    std::string value;
    if (cursor.moveToRange(upperBound, key, value)) {
        if (!cursor.movePrev(key, value))
            return std::nullopt;
    } else if (!cursor.moveLast(key, value)) {
        return std::nullopt;
    }

    if (!key.starts_with(keyPrefix))
        return std::nullopt;

    const auto index = room_store::orderedIndexFromKey(db, roomId, key);
    if (!index)
        return std::nullopt;

    return std::pair<std::uint64_t, std::string>{*index, std::move(value)};
}

inline void
forEachOrderedEntryFrom(
  db::Transaction &txn,
  db::Store &store,
  cache::schema::RoomDb db,
  std::string_view roomId,
  std::uint64_t startIndex,
  db::ScanDirection direction,
  const std::function<bool(std::uint64_t index, std::string_view value)> &visitor)
{
    auto cursor = db::Cursor::open(txn, store);

    const auto keyPrefix = room_store::prefix(db, roomId);
    std::string key;
    std::string value;
    const auto startKey = room_store::orderedIndexKey(db, roomId, startIndex);

    if (direction == db::ScanDirection::Forward) {
        if (!cursor.moveToRange(startKey, key, value) || !key.starts_with(keyPrefix))
            return;
    } else if (cursor.moveTo(startKey, key, value)) {
        // exact match found
    } else if (cursor.moveToRange(startKey, key, value)) {
        if (!cursor.movePrev(key, value))
            return;
    } else if (!cursor.moveLast(key, value)) {
        return;
    }

    while (key.starts_with(keyPrefix)) {
        const auto index = room_store::orderedIndexFromKey(db, roomId, key);
        if (!index)
            break;
        if (!visitor(*index, value))
            break;

        if (direction == db::ScanDirection::Forward) {
            if (!cursor.moveNext(key, value))
                break;
        } else if (!cursor.movePrev(key, value)) {
            break;
        }
    }
}

inline void
forEachOrderedEntry(db::Transaction &txn,
                    db::Store &store,
                    cache::schema::RoomDb db,
                    std::string_view roomId,
                    const std::function<bool(std::uint64_t index, std::string_view value)> &visitor)
{
    if (const auto first = firstOrderedEntry(txn, store, db, roomId); first) {
        forEachOrderedEntryFrom(
          txn, store, db, roomId, first->first, db::ScanDirection::Forward, visitor);
    }
}

inline std::vector<std::pair<std::uint64_t, std::string>>
listOrderEntriesAfterPrevBatchMarker(db::Transaction &txn,
                                     db::Store &eventOrderDb,
                                     std::string_view roomId)
{
    std::vector<std::pair<std::uint64_t, std::string>> orderEntriesToDelete;
    bool passedPaginationToken = false;

    const auto lastOrder =
      lastOrderedEntry(txn, eventOrderDb, cache::schema::RoomDb::EventOrder, roomId);
    if (!lastOrder)
        return orderEntriesToDelete;

    forEachOrderedEntryFrom(txn,
                            eventOrderDb,
                            cache::schema::RoomDb::EventOrder,
                            roomId,
                            lastOrder->first,
                            db::ScanDirection::Backward,
                            [&orderEntriesToDelete, &passedPaginationToken](
                              std::uint64_t index, std::string_view orderValue) {
                                const auto entry = db::parseOrderEntry(orderValue);
                                if (passedPaginationToken) {
                                    orderEntriesToDelete.emplace_back(index,
                                                                      std::string(orderValue));
                                } else if (entry.hasPrevBatch) {
                                    passedPaginationToken = true;
                                }
                                return true;
                            });

    return orderEntriesToDelete;
}

inline void
removeOrderEntryReferences(db::Transaction &txn,
                           db::Store &eventsDb,
                           db::Store &relationsDb,
                           db::Store &eventToOrderDb,
                           db::Store &messageToOrderDb,
                           db::Store &orderToMessageDb,
                           std::string_view roomId,
                           std::string_view orderEntryValue)
{
    const auto entry = db::parseOrderEntry(orderEntryValue);
    if (!entry.eventId)
        return;

    removeTimelineEventReferences(txn,
                                  eventsDb,
                                  relationsDb,
                                  eventToOrderDb,
                                  messageToOrderDb,
                                  orderToMessageDb,
                                  roomId,
                                  *entry.eventId);
}

inline void
removeOrderEntryWithReferences(db::Transaction &txn,
                               db::Store &eventOrderDb,
                               db::Store &eventsDb,
                               db::Store &relationsDb,
                               db::Store &eventToOrderDb,
                               db::Store &messageToOrderDb,
                               db::Store &orderToMessageDb,
                               std::string_view roomId,
                               std::uint64_t orderIndex,
                               std::string_view orderEntryValue)
{
    removeOrderEntryReferences(txn,
                               eventsDb,
                               relationsDb,
                               eventToOrderDb,
                               messageToOrderDb,
                               orderToMessageDb,
                               roomId,
                               orderEntryValue);
    eventOrderDb.del(
      txn, room_store::orderedIndexKey(cache::schema::RoomDb::EventOrder, roomId, orderIndex));
}

} // namespace detail

inline bool
removeMessageOrderMapping(db::Transaction &txn,
                          db::Store &messageToOrderDb,
                          db::Store &orderToMessageDb,
                          std::string_view roomId,
                          std::string_view eventId)
{
    std::string_view messageOrder;
    if (!room_store::get(txn,
                         messageToOrderDb,
                         cache::schema::RoomDb::MessageToOrder,
                         roomId,
                         eventId,
                         messageOrder)) {
        return false;
    }

    orderToMessageDb.del(
      txn, room_store::key(cache::schema::RoomDb::OrderToMessage, roomId, messageOrder));
    room_store::del(txn, messageToOrderDb, cache::schema::RoomDb::MessageToOrder, roomId, eventId);
    return true;
}

inline bool
replaceTimelineEventId(db::Transaction &txn,
                       db::Store &eventsDb,
                       db::Store &eventOrderDb,
                       db::Store &eventToOrderDb,
                       db::Store &messageToOrderDb,
                       db::Store &orderToMessageDb,
                       std::string_view roomId,
                       std::string_view oldEventId,
                       std::string_view newEventId,
                       std::string_view eventJson,
                       std::string_view orderEntryValue)
{
    if (oldEventId.empty() || newEventId.empty())
        return false;

    std::string_view eventOrder;
    if (!room_store::get(txn,
                         eventToOrderDb,
                         cache::schema::RoomDb::EventToOrder,
                         roomId,
                         oldEventId,
                         eventOrder)) {
        return false;
    }

    if (oldEventId == newEventId) {
        room_store::put(
          txn, eventsDb, cache::schema::RoomDb::Events, roomId, newEventId, eventJson);
        eventOrderDb.put(txn,
                         room_store::key(cache::schema::RoomDb::EventOrder, roomId, eventOrder),
                         orderEntryValue);
        return true;
    }

    room_store::put(txn, eventsDb, cache::schema::RoomDb::Events, roomId, newEventId, eventJson);
    room_store::del(txn, eventsDb, cache::schema::RoomDb::Events, roomId, oldEventId);

    std::string_view messageOrder;
    if (room_store::get(txn,
                        messageToOrderDb,
                        cache::schema::RoomDb::MessageToOrder,
                        roomId,
                        oldEventId,
                        messageOrder)) {
        orderToMessageDb.put(
          txn,
          room_store::key(cache::schema::RoomDb::OrderToMessage, roomId, messageOrder),
          newEventId);
        room_store::put(txn,
                        messageToOrderDb,
                        cache::schema::RoomDb::MessageToOrder,
                        roomId,
                        newEventId,
                        messageOrder);
        room_store::del(
          txn, messageToOrderDb, cache::schema::RoomDb::MessageToOrder, roomId, oldEventId);
    }

    eventOrderDb.put(
      txn, room_store::key(cache::schema::RoomDb::EventOrder, roomId, eventOrder), orderEntryValue);
    room_store::put(
      txn, eventToOrderDb, cache::schema::RoomDb::EventToOrder, roomId, newEventId, eventOrder);
    room_store::del(txn, eventToOrderDb, cache::schema::RoomDb::EventToOrder, roomId, oldEventId);
    return true;
}

inline void
putEventOrderMapping(db::Transaction &txn,
                     db::Store &eventOrderDb,
                     db::Store &eventToOrderDb,
                     std::string_view roomId,
                     std::uint64_t eventOrder,
                     std::string_view eventId,
                     std::string_view orderEntryValue,
                     db::PutFlags eventOrderPutFlags = db::PutFlags::None)
{
    eventOrderDb.put(
      txn,
      room_store::orderedIndexKey(cache::schema::RoomDb::EventOrder, roomId, eventOrder),
      orderEntryValue,
      detail::sanitizeOrderedPutFlags(eventOrderPutFlags));
    room_store::put(txn,
                    eventToOrderDb,
                    cache::schema::RoomDb::EventToOrder,
                    roomId,
                    eventId,
                    db::toSv(eventOrder));
}

inline void
putOrderEntry(db::Transaction &txn,
              db::Store &eventOrderDb,
              std::string_view roomId,
              std::uint64_t eventOrder,
              std::string_view eventId,
              std::optional<std::string_view> prevBatch = std::nullopt,
              db::PutFlags eventOrderPutFlags           = db::PutFlags::None)
{
    eventOrderDb.put(
      txn,
      room_store::orderedIndexKey(cache::schema::RoomDb::EventOrder, roomId, eventOrder),
      db::serializeOrderEntry(eventId, prevBatch),
      detail::sanitizeOrderedPutFlags(eventOrderPutFlags));
}

inline void
putEventOrderMappingForEvent(db::Transaction &txn,
                             db::Store &eventOrderDb,
                             db::Store &eventToOrderDb,
                             std::string_view roomId,
                             std::uint64_t eventOrder,
                             std::string_view eventId,
                             std::optional<std::string_view> prevBatch = std::nullopt,
                             db::PutFlags eventOrderPutFlags           = db::PutFlags::None)
{
    putEventOrderMapping(txn,
                         eventOrderDb,
                         eventToOrderDb,
                         roomId,
                         eventOrder,
                         eventId,
                         db::serializeOrderEntry(eventId, prevBatch),
                         eventOrderPutFlags);
}

inline void
putMessageOrderMapping(db::Transaction &txn,
                       db::Store &orderToMessageDb,
                       db::Store &messageToOrderDb,
                       std::string_view roomId,
                       std::uint64_t messageOrder,
                       std::string_view eventId,
                       db::PutFlags orderToMessagePutFlags = db::PutFlags::None)
{
    orderToMessageDb.put(
      txn,
      room_store::orderedIndexKey(cache::schema::RoomDb::OrderToMessage, roomId, messageOrder),
      eventId,
      detail::sanitizeOrderedPutFlags(orderToMessagePutFlags));
    room_store::put(txn,
                    messageToOrderDb,
                    cache::schema::RoomDb::MessageToOrder,
                    roomId,
                    eventId,
                    db::toSv(messageOrder));
}

inline std::uint64_t
appendEventOrderEntry(db::Transaction &txn,
                      db::Store &eventOrderDb,
                      db::Store &eventToOrderDb,
                      std::string_view roomId,
                      std::uint64_t &lastEventOrder,
                      std::string_view eventId,
                      std::string_view orderEntryValue)
{
    lastEventOrder += 1;
    putEventOrderMapping(txn,
                         eventOrderDb,
                         eventToOrderDb,
                         roomId,
                         lastEventOrder,
                         eventId,
                         orderEntryValue,
                         db::PutFlags::Append);
    return lastEventOrder;
}

inline std::uint64_t
prependEventOrderEntry(db::Transaction &txn,
                       db::Store &eventOrderDb,
                       db::Store &eventToOrderDb,
                       std::string_view roomId,
                       std::uint64_t &firstEventOrder,
                       std::string_view eventId,
                       std::string_view orderEntryValue)
{
    firstEventOrder -= 1;
    putEventOrderMapping(
      txn, eventOrderDb, eventToOrderDb, roomId, firstEventOrder, eventId, orderEntryValue);
    return firstEventOrder;
}

inline std::uint64_t
appendMessageOrderEntry(db::Transaction &txn,
                        db::Store &orderToMessageDb,
                        db::Store &messageToOrderDb,
                        std::string_view roomId,
                        std::uint64_t &lastMessageOrder,
                        std::string_view eventId)
{
    lastMessageOrder += 1;
    putMessageOrderMapping(txn,
                           orderToMessageDb,
                           messageToOrderDb,
                           roomId,
                           lastMessageOrder,
                           eventId,
                           db::PutFlags::Append);
    return lastMessageOrder;
}

inline std::uint64_t
prependMessageOrderEntry(db::Transaction &txn,
                         db::Store &orderToMessageDb,
                         db::Store &messageToOrderDb,
                         std::string_view roomId,
                         std::uint64_t &firstMessageOrder,
                         std::string_view eventId)
{
    firstMessageOrder -= 1;
    putMessageOrderMapping(
      txn, orderToMessageDb, messageToOrderDb, roomId, firstMessageOrder, eventId);
    return firstMessageOrder;
}

inline std::optional<std::pair<std::uint64_t, std::string>>
lastInvisibleEventAfter(db::Transaction &txn,
                        db::Store &eventToOrderDb,
                        db::Store &eventOrderDb,
                        db::Store &messageToOrderDb,
                        std::string_view roomId,
                        std::string_view eventId)
{
    if (eventId.empty())
        return std::nullopt;

    const auto startIndex = eventIndexForEvent(txn, eventToOrderDb, roomId, eventId);
    if (!startIndex)
        return std::nullopt;

    std::uint64_t previousIndex = *startIndex;
    std::string previousEventId{eventId};

    std::optional<std::pair<std::uint64_t, std::string>> result;
    detail::forEachOrderedEntryFrom(
      txn,
      eventOrderDb,
      cache::schema::RoomDb::EventOrder,
      roomId,
      *startIndex,
      db::ScanDirection::Forward,
      [&result, &txn, &messageToOrderDb, &roomId, &previousIndex, &previousEventId](
        std::uint64_t index, std::string_view value) {
          const auto eventFromOrder = db::parseOrderEntry(value).eventId.value_or("");
          std::string_view ignored;
          if (room_store::get(txn,
                              messageToOrderDb,
                              cache::schema::RoomDb::MessageToOrder,
                              roomId,
                              eventFromOrder,
                              ignored)) {
              result = std::pair{previousIndex, previousEventId};
              return false;
          }

          previousIndex   = index;
          previousEventId = eventFromOrder;
          return true;
      });

    if (result)
        return result;

    return std::pair{previousIndex, previousEventId};
}

inline std::optional<std::pair<std::uint64_t, std::string>>
lastVisibleEvent(db::Transaction &txn,
                 db::Store &eventToOrderDb,
                 db::Store &eventOrderDb,
                 db::Store &messageToOrderDb,
                 std::string_view roomId,
                 std::string_view eventId)
{
    if (eventId.empty())
        return std::nullopt;

    const auto startIndex = eventIndexForEvent(txn, eventToOrderDb, roomId, eventId);
    if (!startIndex)
        return std::nullopt;

    std::uint64_t index = *startIndex;
    std::string eventIdAtIndex{eventId};

    std::optional<std::pair<std::uint64_t, std::string>> result;
    detail::forEachOrderedEntryFrom(
      txn,
      eventOrderDb,
      cache::schema::RoomDb::EventOrder,
      roomId,
      *startIndex,
      db::ScanDirection::Backward,
      [&result, &txn, &messageToOrderDb, &roomId, &index, &eventIdAtIndex](std::uint64_t entryIndex,
                                                                           std::string_view value) {
          eventIdAtIndex = db::parseOrderEntry(value).eventId.value_or("");
          index          = entryIndex;

          std::string_view ignored;
          if (room_store::get(txn,
                              messageToOrderDb,
                              cache::schema::RoomDb::MessageToOrder,
                              roomId,
                              eventIdAtIndex,
                              ignored)) {
              result = std::pair{index, eventIdAtIndex};
              return false;
          }
          return true;
      });

    if (result)
        return result;

    return std::pair{index, eventIdAtIndex};
}

inline std::optional<std::string>
lastTimelineEventId(db::Transaction &txn, db::Store &orderToMessageDb, std::string_view roomId)
{
    const auto last = detail::lastOrderedEntry(
      txn, orderToMessageDb, cache::schema::RoomDb::OrderToMessage, roomId);
    if (!last)
        return std::nullopt;

    return last->second;
}

inline std::optional<std::pair<std::uint64_t, std::uint64_t>>
timelineRange(db::Transaction &txn, db::Store &orderToMessageDb, std::string_view roomId)
{
    const auto first = detail::firstOrderedEntry(
      txn, orderToMessageDb, cache::schema::RoomDb::OrderToMessage, roomId);
    if (!first)
        return std::nullopt;

    const auto last = detail::lastOrderedEntry(
      txn, orderToMessageDb, cache::schema::RoomDb::OrderToMessage, roomId);
    if (!last)
        return std::nullopt;

    return std::pair{first->first, last->first};
}

inline std::optional<std::uint64_t>
timelineIndexForEvent(db::Transaction &txn,
                      db::Store &messageToOrderDb,
                      std::string_view roomId,
                      std::string_view eventId)
{
    if (eventId.empty())
        return std::nullopt;

    std::string_view value;
    if (!room_store::get(
          txn, messageToOrderDb, cache::schema::RoomDb::MessageToOrder, roomId, eventId, value)) {
        return std::nullopt;
    }

    return db::fromSv<std::uint64_t>(value);
}

inline std::optional<std::uint64_t>
eventIndexForEvent(db::Transaction &txn,
                   db::Store &eventToOrderDb,
                   std::string_view roomId,
                   std::string_view eventId)
{
    if (eventId.empty())
        return std::nullopt;

    std::string_view value;
    if (!room_store::get(
          txn, eventToOrderDb, cache::schema::RoomDb::EventToOrder, roomId, eventId, value)) {
        return std::nullopt;
    }

    return db::fromSv<std::uint64_t>(value);
}

inline std::optional<std::string>
timelineEventIdAtIndex(db::Transaction &txn,
                       db::Store &orderToMessageDb,
                       std::string_view roomId,
                       std::uint64_t index)
{
    std::string_view value;
    if (!orderToMessageDb.get(
          txn,
          room_store::orderedIndexKey(cache::schema::RoomDb::OrderToMessage, roomId, index),
          value)) {
        return std::nullopt;
    }

    return std::string(value);
}

inline std::optional<std::uint64_t>
firstOrderedIndex(db::Transaction &txn,
                  db::Store &orderedDb,
                  cache::schema::RoomDb db,
                  std::string_view roomId)
{
    const auto first = detail::firstOrderedEntry(txn, orderedDb, db, roomId);
    if (!first)
        return std::nullopt;

    return first->first;
}

inline std::optional<std::uint64_t>
lastOrderedIndex(db::Transaction &txn,
                 db::Store &orderedDb,
                 cache::schema::RoomDb db,
                 std::string_view roomId)
{
    const auto last = detail::lastOrderedEntry(txn, orderedDb, db, roomId);
    if (!last)
        return std::nullopt;

    return last->first;
}

inline std::optional<std::string>
firstPrevBatchToken(db::Transaction &txn, db::Store &eventOrderDb, std::string_view roomId)
{
    const auto first =
      detail::firstOrderedEntry(txn, eventOrderDb, cache::schema::RoomDb::EventOrder, roomId);
    if (!first)
        return std::nullopt;

    return db::parseOrderEntry(first->second).prevBatch;
}

inline bool
setOrderEntryPrevBatch(db::Transaction &txn,
                       db::Store &eventOrderDb,
                       std::string_view roomId,
                       std::uint64_t eventOrder,
                       std::string_view prevBatch)
{
    std::string_view rawEntry;
    if (!eventOrderDb.get(
          txn,
          room_store::orderedIndexKey(cache::schema::RoomDb::EventOrder, roomId, eventOrder),
          rawEntry)) {
        return false;
    }

    auto entry         = db::parseOrderEntry(rawEntry);
    entry.hasPrevBatch = true;
    entry.prevBatch    = std::string(prevBatch);
    const auto encoded = db::serializeOrderEntry(entry);
    return eventOrderDb.put(
      txn,
      room_store::orderedIndexKey(cache::schema::RoomDb::EventOrder, roomId, eventOrder),
      encoded);
}

inline std::size_t
removePendingEntriesByTxnId(db::Transaction &txn,
                            db::Store &pendingDb,
                            std::string_view roomId,
                            std::string_view txnId)
{
    std::vector<std::pair<std::string, std::string>> entriesToDelete;
    room_store::forEachEntry(
      txn,
      pendingDb,
      cache::schema::RoomDb::Pending,
      roomId,
      [&entriesToDelete, txnId](std::string_view timestamp, std::string_view pendingTxn) {
          if (pendingTxn == txnId)
              entriesToDelete.emplace_back(std::string(timestamp), std::string(pendingTxn));
          return true;
      });

    for (const auto &[timestamp, pendingTxn] : entriesToDelete)
        room_store::del(
          txn, pendingDb, cache::schema::RoomDb::Pending, roomId, timestamp, pendingTxn);

    return entriesToDelete.size();
}

inline std::size_t
removeRelationSourceReferences(db::Transaction &txn,
                               db::Store &relationsDb,
                               std::string_view roomId,
                               std::string_view sourceEventId)
{
    if (sourceEventId.empty())
        return 0;

    std::vector<std::pair<std::string, std::string>> entriesToDelete;
    room_store::forEachEntry(txn,
                             relationsDb,
                             cache::schema::RoomDb::Related,
                             roomId,
                             [&entriesToDelete, sourceEventId](std::string_view targetEventId,
                                                               std::string_view relatedEventId) {
                                 if (relatedEventId == sourceEventId)
                                     entriesToDelete.emplace_back(std::string(targetEventId),
                                                                  std::string(relatedEventId));
                                 return true;
                             });

    for (const auto &[targetEventId, relatedEventId] : entriesToDelete) {
        room_store::del(
          txn, relationsDb, cache::schema::RoomDb::Related, roomId, targetEventId, relatedEventId);
    }

    return entriesToDelete.size();
}

inline std::size_t
rewriteRelationSourceReferences(db::Transaction &txn,
                                db::Store &relationsDb,
                                std::string_view roomId,
                                std::string_view sourceEventId,
                                std::span<const std::string_view> targetEventIds)
{
    if (sourceEventId.empty())
        return 0;

    const auto removed = removeRelationSourceReferences(txn, relationsDb, roomId, sourceEventId);

    std::size_t written = 0;
    for (const auto &targetEventId : targetEventIds) {
        if (targetEventId.empty())
            continue;
        relationsDb.put(txn,
                        room_store::key(cache::schema::RoomDb::Related, roomId, targetEventId),
                        sourceEventId);
        written += 1;
    }

    return removed + written;
}

inline std::vector<std::string>
listOrderEntryEventIds(db::Transaction &txn, db::Store &eventOrderDb, std::string_view roomId)
{
    std::vector<std::string> eventIds;
    room_store::forEachEntry(txn,
                             eventOrderDb,
                             cache::schema::RoomDb::EventOrder,
                             roomId,
                             [&eventIds](std::string_view, std::string_view orderValue) {
                                 const auto entry = db::parseOrderEntry(orderValue);
                                 if (entry.eventId)
                                     eventIds.emplace_back(*entry.eventId);
                                 return true;
                             });

    return eventIds;
}

inline std::size_t
removeMessageOrderMappingsNotInOrderEntries(db::Transaction &txn,
                                            db::Store &eventOrderDb,
                                            db::Store &orderToMessageDb,
                                            db::Store &messageToOrderDb,
                                            std::string_view roomId)
{
    std::unordered_set<std::string> expectedEventIds;
    for (const auto &eventId : listOrderEntryEventIds(txn, eventOrderDb, roomId))
        expectedEventIds.insert(eventId);

    std::vector<std::string> staleEventIds;
    room_store::forEachEntry(
      txn,
      orderToMessageDb,
      cache::schema::RoomDb::OrderToMessage,
      roomId,
      [&expectedEventIds, &staleEventIds](std::string_view, std::string_view eventId) {
          if (!expectedEventIds.contains(std::string(eventId)))
              staleEventIds.emplace_back(eventId);
          return true;
      });

    for (const auto &eventId : staleEventIds)
        removeMessageOrderMapping(txn, messageToOrderDb, orderToMessageDb, roomId, eventId);

    return staleEventIds.size();
}

inline void
removeTimelineEventReferences(db::Transaction &txn,
                              db::Store &eventsDb,
                              db::Store &relationsDb,
                              db::Store &eventToOrderDb,
                              db::Store &messageToOrderDb,
                              db::Store &orderToMessageDb,
                              std::string_view roomId,
                              std::string_view eventId)
{
    room_store::del(txn, eventToOrderDb, cache::schema::RoomDb::EventToOrder, roomId, eventId);
    room_store::del(txn, eventsDb, cache::schema::RoomDb::Events, roomId, eventId);
    removeRelationSourceReferences(txn, relationsDb, roomId, eventId);
    relationsDb.del(txn, room_store::key(cache::schema::RoomDb::Related, roomId, eventId));
    removeMessageOrderMapping(txn, messageToOrderDb, orderToMessageDb, roomId, eventId);
}

inline std::size_t
trimOldestOrderEntriesWithReferences(db::Transaction &txn,
                                     db::Store &eventOrderDb,
                                     db::Store &eventsDb,
                                     db::Store &relationsDb,
                                     db::Store &eventToOrderDb,
                                     db::Store &messageToOrderDb,
                                     db::Store &orderToMessageDb,
                                     std::string_view roomId,
                                     std::size_t count)
{
    if (count == 0)
        return 0;

    std::vector<std::pair<std::uint64_t, std::string>> entriesToDelete;
    room_store::forEachEntry(
      txn,
      eventOrderDb,
      cache::schema::RoomDb::EventOrder,
      roomId,
      0,
      count,
      [&entriesToDelete](std::string_view rawOrderIndex, std::string_view orderEntryValue) {
          const auto orderIndex = room_store::orderedIndexFromSubkey(rawOrderIndex);
          if (orderIndex)
              entriesToDelete.emplace_back(*orderIndex, std::string(orderEntryValue));
          return true;
      });

    for (const auto &[orderIndex, orderEntryValue] : entriesToDelete) {
        detail::removeOrderEntryWithReferences(txn,
                                               eventOrderDb,
                                               eventsDb,
                                               relationsDb,
                                               eventToOrderDb,
                                               messageToOrderDb,
                                               orderToMessageDb,
                                               roomId,
                                               orderIndex,
                                               orderEntryValue);
    }

    return entriesToDelete.size();
}

inline void
cleanupTimelineBeforePrevBatchMarker(db::Transaction &txn,
                                     db::Store &eventOrderDb,
                                     db::Store &eventsDb,
                                     db::Store &relationsDb,
                                     db::Store &eventToOrderDb,
                                     db::Store &messageToOrderDb,
                                     db::Store &orderToMessageDb,
                                     std::string_view roomId)
{
    const auto orderEntriesToDelete =
      detail::listOrderEntriesAfterPrevBatchMarker(txn, eventOrderDb, roomId);
    for (const auto &[orderIndex, orderEntryValue] : orderEntriesToDelete) {
        detail::removeOrderEntryWithReferences(txn,
                                               eventOrderDb,
                                               eventsDb,
                                               relationsDb,
                                               eventToOrderDb,
                                               messageToOrderDb,
                                               orderToMessageDb,
                                               roomId,
                                               orderIndex,
                                               orderEntryValue);
    }

    removeMessageOrderMappingsNotInOrderEntries(
      txn, eventOrderDb, orderToMessageDb, messageToOrderDb, roomId);
}

} // namespace cache::room_timeline

namespace room_timeline = cache::room_timeline;
