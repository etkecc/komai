// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "db/TimelineIndex.h"

#include "db/DupIndex.h"
#include <unordered_set>

#include "db/DbTypes.h"
#include "db/OrderEntry.h"
#include "db/Scan.h"

namespace db {

std::size_t
removeMessageOrderMappingsNotInOrderEntries(Txn &txn,
                                            Store &eventOrderDb,
                                            Store &orderToMessageDb,
                                            Store &messageToOrderDb)
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

std::size_t
removeRelationSourceReferences(Txn &txn, Store &relationsDb, std::string_view sourceEventId)
{
    if (sourceEventId.empty())
        return 0;

    return eraseEntriesIf(
      txn,
      relationsDb,
      [sourceEventId](std::string_view /*targetEventId*/, std::string_view relatedEventId) {
          return relatedEventId == sourceEventId;
      });
}

std::size_t
rewriteRelationSourceReferences(Txn &txn,
                                Store &relationsDb,
                                std::string_view sourceEventId,
                                std::span<const std::string_view> targetEventIds)
{
    if (sourceEventId.empty())
        return 0;

    const auto removed = removeRelationSourceReferences(txn, relationsDb, sourceEventId);
    const auto written = putDupValueForKeys(txn, relationsDb, targetEventIds, sourceEventId);
    return removed + written;
}

void
removeOrderEntryReferences(Txn &txn,
                           Store &eventsDb,
                           Store &relationsDb,
                           Store &eventToOrderDb,
                           Store &messageToOrderDb,
                           Store &orderToMessageDb,
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
                               Store &eventOrderDb,
                               Store &eventsDb,
                               Store &relationsDb,
                               Store &eventToOrderDb,
                               Store &messageToOrderDb,
                               Store &orderToMessageDb,
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
  Store &eventOrderDb,
  Store &eventsDb,
  Store &relationsDb,
  Store &eventToOrderDb,
  Store &messageToOrderDb,
  Store &orderToMessageDb,
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
                                     Store &eventOrderDb,
                                     Store &eventsDb,
                                     Store &relationsDb,
                                     Store &eventToOrderDb,
                                     Store &messageToOrderDb,
                                     Store &orderToMessageDb,
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
                                     Store &eventOrderDb,
                                     Store &eventsDb,
                                     Store &relationsDb,
                                     Store &eventToOrderDb,
                                     Store &messageToOrderDb,
                                     Store &orderToMessageDb)
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
                              Store &eventsDb,
                              Store &relationsDb,
                              Store &eventToOrderDb,
                              Store &messageToOrderDb,
                              Store &orderToMessageDb,
                              std::string_view eventId)
{
    eventToOrderDb.del(txn, eventId);
    eventsDb.del(txn, eventId);
    removeRelationSourceReferences(txn, relationsDb, eventId);
    relationsDb.del(txn, eventId);
    removeMessageOrderMapping(txn, messageToOrderDb, orderToMessageDb, eventId);
}

} // namespace db
