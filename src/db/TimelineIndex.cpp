// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "db/TimelineIndex.h"

#include <unordered_set>

#include "db/DbTypes.h"
#include "db/OrderEntry.h"
#include "db/Scan.h"

namespace db {

bool
removeMessageOrderMapping(Txn &txn,
                          Dbi &messageToOrderDb,
                          Dbi &orderToMessageDb,
                          std::string_view eventId)
{
    std::string_view messageOrder;
    if (!messageToOrderDb.get(txn, eventId, messageOrder))
        return false;

    orderToMessageDb.del(txn, messageOrder);
    messageToOrderDb.del(txn, eventId);
    return true;
}

bool
replaceTimelineEventId(Txn &txn,
                       Dbi &eventsDb,
                       Dbi &eventOrderDb,
                       Dbi &eventToOrderDb,
                       Dbi &messageToOrderDb,
                       Dbi &orderToMessageDb,
                       std::string_view oldEventId,
                       std::string_view newEventId,
                       std::string_view eventJson,
                       std::string_view orderEntryValue)
{
    if (oldEventId.empty() || newEventId.empty())
        return false;

    std::string_view eventOrder;
    if (!eventToOrderDb.get(txn, oldEventId, eventOrder))
        return false;

    if (oldEventId == newEventId) {
        eventsDb.put(txn, newEventId, eventJson);
        eventOrderDb.put(txn, eventOrder, orderEntryValue);
        return true;
    }

    eventsDb.put(txn, newEventId, eventJson);
    eventsDb.del(txn, oldEventId);

    std::string_view messageOrder;
    if (messageToOrderDb.get(txn, oldEventId, messageOrder)) {
        orderToMessageDb.put(txn, messageOrder, newEventId);
        messageToOrderDb.put(txn, newEventId, messageOrder);
        messageToOrderDb.del(txn, oldEventId);
    }

    eventOrderDb.put(txn, eventOrder, orderEntryValue);
    eventToOrderDb.put(txn, newEventId, eventOrder);
    eventToOrderDb.del(txn, oldEventId);
    return true;
}

void
putEventOrderMapping(Txn &txn,
                     Dbi &eventOrderDb,
                     Dbi &eventToOrderDb,
                     std::uint64_t eventOrder,
                     std::string_view eventId,
                     std::string_view orderEntryValue,
                     PutFlags eventOrderPutFlags)
{
    eventOrderDb.put(txn, toSv(eventOrder), orderEntryValue, eventOrderPutFlags);
    eventToOrderDb.put(txn, eventId, toSv(eventOrder));
}

void
putOrderEntry(Txn &txn,
              Dbi &eventOrderDb,
              std::uint64_t eventOrder,
              std::string_view eventId,
              std::optional<std::string_view> prevBatch,
              PutFlags eventOrderPutFlags)
{
    eventOrderDb.put(
      txn, toSv(eventOrder), serializeOrderEntry(eventId, prevBatch), eventOrderPutFlags);
}

void
putEventOrderMappingForEvent(Txn &txn,
                             Dbi &eventOrderDb,
                             Dbi &eventToOrderDb,
                             std::uint64_t eventOrder,
                             std::string_view eventId,
                             std::optional<std::string_view> prevBatch,
                             PutFlags eventOrderPutFlags)
{
    putEventOrderMapping(txn,
                         eventOrderDb,
                         eventToOrderDb,
                         eventOrder,
                         eventId,
                         serializeOrderEntry(eventId, prevBatch),
                         eventOrderPutFlags);
}

void
putMessageOrderMapping(Txn &txn,
                       Dbi &orderToMessageDb,
                       Dbi &messageToOrderDb,
                       std::uint64_t messageOrder,
                       std::string_view eventId,
                       PutFlags orderToMessagePutFlags)
{
    orderToMessageDb.put(txn, toSv(messageOrder), eventId, orderToMessagePutFlags);
    messageToOrderDb.put(txn, eventId, toSv(messageOrder));
}

std::optional<std::pair<std::uint64_t, std::string>>
lastInvisibleEventAfter(Txn &txn,
                        Dbi &eventToOrderDb,
                        Dbi &eventOrderDb,
                        Dbi &messageToOrderDb,
                        std::string_view eventId)
{
    if (eventId.empty())
        return std::nullopt;

    std::string_view indexValue;
    if (!eventToOrderDb.get(txn, eventId, indexValue))
        return std::nullopt;

    std::uint64_t previousIndex = fromSv<std::uint64_t>(indexValue);
    std::string previousEventId{eventId};

    std::optional<std::pair<std::uint64_t, std::string>> result;
    forEachEntryFromKey(txn,
                        eventOrderDb,
                        indexValue,
                        ScanDirection::Forward,
                        [&](std::string_view key, std::string_view value) {
                            const auto eventFromOrder = parseOrderEntry(value).eventId.value_or("");
                            std::string_view ignored;
                            if (messageToOrderDb.get(txn, eventFromOrder, ignored)) {
                                result = std::pair{previousIndex, previousEventId};
                                return false;
                            }

                            previousIndex   = fromSv<std::uint64_t>(key);
                            previousEventId = eventFromOrder;
                            return true;
                        });

    if (result)
        return result;

    return std::pair{previousIndex, previousEventId};
}

std::optional<std::pair<std::uint64_t, std::string>>
lastVisibleEvent(Txn &txn,
                 Dbi &eventToOrderDb,
                 Dbi &eventOrderDb,
                 Dbi &messageToOrderDb,
                 std::string_view eventId)
{
    if (eventId.empty())
        return std::nullopt;

    std::string_view indexValue;
    if (!eventToOrderDb.get(txn, eventId, indexValue))
        return std::nullopt;

    std::uint64_t index = fromSv<std::uint64_t>(indexValue);
    std::string eventIdAtIndex{eventId};

    std::optional<std::pair<std::uint64_t, std::string>> result;
    forEachEntryFromKey(txn,
                        eventOrderDb,
                        indexValue,
                        ScanDirection::Backward,
                        [&](std::string_view key, std::string_view value) {
                            eventIdAtIndex = parseOrderEntry(value).eventId.value_or("");
                            index          = fromSv<std::uint64_t>(key);

                            std::string_view ignored;
                            if (messageToOrderDb.get(txn, eventIdAtIndex, ignored)) {
                                result = std::pair{index, eventIdAtIndex};
                                return false;
                            }
                            return true;
                        });

    if (result)
        return result;

    return std::pair{index, eventIdAtIndex};
}

std::optional<std::string>
lastTimelineEventId(Txn &txn, Dbi &orderToMessageDb)
{
    const auto last = lastEntry(txn, orderToMessageDb);
    if (!last)
        return std::nullopt;

    return last->second;
}

std::optional<std::pair<std::uint64_t, std::uint64_t>>
timelineRange(Txn &txn, Dbi &orderToMessageDb)
{
    const auto first = firstEntry(txn, orderToMessageDb);
    if (!first)
        return std::nullopt;

    const auto last = lastEntry(txn, orderToMessageDb);
    if (!last)
        return std::nullopt;

    return std::pair{fromSv<std::uint64_t>(first->first), fromSv<std::uint64_t>(last->first)};
}

std::optional<std::uint64_t>
timelineIndexForEvent(Txn &txn, Dbi &messageToOrderDb, std::string_view eventId)
{
    if (eventId.empty())
        return std::nullopt;

    std::string_view value;
    if (!messageToOrderDb.get(txn, eventId, value))
        return std::nullopt;

    return fromSv<std::uint64_t>(value);
}

std::optional<std::uint64_t>
eventIndexForEvent(Txn &txn, Dbi &eventToOrderDb, std::string_view eventId)
{
    if (eventId.empty())
        return std::nullopt;

    std::string_view value;
    if (!eventToOrderDb.get(txn, eventId, value))
        return std::nullopt;

    return fromSv<std::uint64_t>(value);
}

std::optional<std::string>
timelineEventIdAtIndex(Txn &txn, Dbi &orderToMessageDb, std::uint64_t index)
{
    std::string_view value;
    if (!orderToMessageDb.get(txn, toSv(index), value))
        return std::nullopt;

    return std::string(value);
}

std::optional<std::uint64_t>
firstOrderedIndex(Txn &txn, Dbi &orderedDb)
{
    const auto first = firstEntry(txn, orderedDb);
    if (!first)
        return std::nullopt;

    return fromSv<std::uint64_t>(first->first);
}

std::optional<std::uint64_t>
lastOrderedIndex(Txn &txn, Dbi &orderedDb)
{
    const auto last = lastEntry(txn, orderedDb);
    if (!last)
        return std::nullopt;

    return fromSv<std::uint64_t>(last->first);
}

std::optional<std::string>
firstPrevBatchToken(Txn &txn, Dbi &eventOrderDb)
{
    const auto first = firstEntry(txn, eventOrderDb);
    if (!first)
        return std::nullopt;

    return parseOrderEntry(first->second).prevBatch;
}

bool
setOrderEntryPrevBatch(Txn &txn,
                       Dbi &eventOrderDb,
                       std::uint64_t eventOrder,
                       std::string_view prevBatch)
{
    std::string_view rawEntry;
    if (!eventOrderDb.get(txn, toSv(eventOrder), rawEntry))
        return false;

    auto entry         = parseOrderEntry(rawEntry);
    entry.hasPrevBatch = true;
    entry.prevBatch    = std::string(prevBatch);
    const auto encoded = serializeOrderEntry(entry);
    return eventOrderDb.put(txn, toSv(eventOrder), encoded);
}

std::size_t
removePendingEntriesByTxnId(Txn &txn, Dbi &pendingDb, std::string_view txnId)
{
    return eraseEntriesIf(
      txn, pendingDb, [txnId](std::string_view /*timestamp*/, std::string_view pendingTxn) {
          return pendingTxn == txnId;
      });
}

std::vector<std::pair<std::string, std::string>>
listOrderEntriesAfterPrevBatchMarker(Txn &txn, Dbi &eventOrderDb)
{
    std::vector<std::pair<std::string, std::string>> orderEntriesToDelete;
    bool passedPaginationToken = false;

    const auto lastOrder = lastEntry(txn, eventOrderDb);
    if (!lastOrder)
        return orderEntriesToDelete;

    forEachEntryFromKey(txn,
                        eventOrderDb,
                        lastOrder->first,
                        ScanDirection::Backward,
                        [&orderEntriesToDelete, &passedPaginationToken](
                          std::string_view orderKey, std::string_view orderValue) {
                            const auto entry = parseOrderEntry(orderValue);
                            if (passedPaginationToken) {
                                orderEntriesToDelete.emplace_back(std::string(orderKey),
                                                                  std::string(orderValue));
                            } else if (entry.hasPrevBatch) {
                                passedPaginationToken = true;
                            }
                            return true;
                        });

    return orderEntriesToDelete;
}

std::vector<std::string>
listOrderEntryEventIds(Txn &txn, Dbi &eventOrderDb)
{
    std::vector<std::string> eventIds;
    forEachEntry(
      txn, eventOrderDb, [&eventIds](std::string_view /*orderKey*/, std::string_view orderValue) {
          const auto entry = parseOrderEntry(orderValue);
          if (entry.eventId)
              eventIds.emplace_back(*entry.eventId);
          return true;
      });

    return eventIds;
}

std::size_t
removeMessageOrderMappingsNotInOrderEntries(Txn &txn,
                                            Dbi &eventOrderDb,
                                            Dbi &orderToMessageDb,
                                            Dbi &messageToOrderDb)
{
    std::unordered_set<std::string> expectedEventIds;
    for (const auto &eventId : listOrderEntryEventIds(txn, eventOrderDb))
        expectedEventIds.insert(eventId);

    std::vector<std::string> staleEventIds;
    eraseEntriesIf(txn,
                   orderToMessageDb,
                   [&expectedEventIds, &staleEventIds](std::string_view /*messageOrder*/,
                                                       std::string_view eventId) {
                       if (expectedEventIds.contains(std::string(eventId)))
                           return false;
                       staleEventIds.emplace_back(eventId);
                       return true;
                   });

    for (const auto &eventId : staleEventIds)
        removeMessageOrderMapping(txn, messageToOrderDb, orderToMessageDb, eventId);

    return staleEventIds.size();
}

void
removeOrderEntryReferences(Txn &txn,
                           Dbi &eventsDb,
                           Dbi &relationsDb,
                           Dbi &eventToOrderDb,
                           Dbi &messageToOrderDb,
                           Dbi &orderToMessageDb,
                           std::string_view orderEntryValue)
{
    const auto entry = parseOrderEntry(orderEntryValue);
    if (!entry.eventId)
        return;

    removeTimelineEventReferences(txn,
                                  eventsDb,
                                  relationsDb,
                                  eventToOrderDb,
                                  messageToOrderDb,
                                  orderToMessageDb,
                                  *entry.eventId);
}

void
removeOrderEntryWithReferences(Txn &txn,
                               Dbi &eventOrderDb,
                               Dbi &eventsDb,
                               Dbi &relationsDb,
                               Dbi &eventToOrderDb,
                               Dbi &messageToOrderDb,
                               Dbi &orderToMessageDb,
                               std::string_view orderKey,
                               std::string_view orderEntryValue)
{
    removeOrderEntryReferences(txn,
                               eventsDb,
                               relationsDb,
                               eventToOrderDb,
                               messageToOrderDb,
                               orderToMessageDb,
                               orderEntryValue);
    eventOrderDb.del(txn, orderKey);
}

std::size_t
eraseOrderEntriesWithReferencesIf(
  Txn &txn,
  Dbi &eventOrderDb,
  Dbi &eventsDb,
  Dbi &relationsDb,
  Dbi &eventToOrderDb,
  Dbi &messageToOrderDb,
  Dbi &orderToMessageDb,
  std::size_t startIndex,
  std::size_t limit,
  const std::function<bool(std::string_view orderKey, std::string_view orderEntryValue)> &predicate)
{
    std::vector<std::pair<std::string, std::string>> entriesToDelete;
    forEachEntry(
      txn,
      eventOrderDb,
      startIndex,
      limit,
      [&entriesToDelete, &predicate](std::string_view orderKey, std::string_view orderEntryValue) {
          if (predicate(orderKey, orderEntryValue))
              entriesToDelete.emplace_back(std::string(orderKey), std::string(orderEntryValue));
          return true;
      });

    for (const auto &[orderKey, orderEntryValue] : entriesToDelete) {
        removeOrderEntryWithReferences(txn,
                                       eventOrderDb,
                                       eventsDb,
                                       relationsDb,
                                       eventToOrderDb,
                                       messageToOrderDb,
                                       orderToMessageDb,
                                       orderKey,
                                       orderEntryValue);
    }

    return entriesToDelete.size();
}

std::size_t
trimOldestOrderEntriesWithReferences(Txn &txn,
                                     Dbi &eventOrderDb,
                                     Dbi &eventsDb,
                                     Dbi &relationsDb,
                                     Dbi &eventToOrderDb,
                                     Dbi &messageToOrderDb,
                                     Dbi &orderToMessageDb,
                                     std::size_t count)
{
    return eraseOrderEntriesWithReferencesIf(
      txn,
      eventOrderDb,
      eventsDb,
      relationsDb,
      eventToOrderDb,
      messageToOrderDb,
      orderToMessageDb,
      0,
      count,
      [](std::string_view /*orderKey*/, std::string_view /*orderEntryValue*/) { return true; });
}

void
cleanupTimelineBeforePrevBatchMarker(Txn &txn,
                                     Dbi &eventOrderDb,
                                     Dbi &eventsDb,
                                     Dbi &relationsDb,
                                     Dbi &eventToOrderDb,
                                     Dbi &messageToOrderDb,
                                     Dbi &orderToMessageDb)
{
    const auto orderEntriesToDelete = listOrderEntriesAfterPrevBatchMarker(txn, eventOrderDb);
    for (const auto &[orderKey, orderEntryValue] : orderEntriesToDelete) {
        removeOrderEntryWithReferences(txn,
                                       eventOrderDb,
                                       eventsDb,
                                       relationsDb,
                                       eventToOrderDb,
                                       messageToOrderDb,
                                       orderToMessageDb,
                                       orderKey,
                                       orderEntryValue);
    }

    removeMessageOrderMappingsNotInOrderEntries(
      txn, eventOrderDb, orderToMessageDb, messageToOrderDb);
}

void
removeTimelineEventReferences(Txn &txn,
                              Dbi &eventsDb,
                              Dbi &relationsDb,
                              Dbi &eventToOrderDb,
                              Dbi &messageToOrderDb,
                              Dbi &orderToMessageDb,
                              std::string_view eventId)
{
    eventToOrderDb.del(txn, eventId);
    eventsDb.del(txn, eventId);
    relationsDb.del(txn, eventId);
    removeMessageOrderMapping(txn, messageToOrderDb, orderToMessageDb, eventId);
}

} // namespace db
