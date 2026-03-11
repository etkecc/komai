// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "db/Flags.h"

namespace db {

class Txn;
class Dbi;

using Transaction = Txn;
using Store       = Dbi;

bool
removeMessageOrderMapping(Transaction &txn,
                          Store &messageToOrderDb,
                          Store &orderToMessageDb,
                          std::string_view eventId);

bool
replaceTimelineEventId(Transaction &txn,
                       Store &eventsDb,
                       Store &eventOrderDb,
                       Store &eventToOrderDb,
                       Store &messageToOrderDb,
                       Store &orderToMessageDb,
                       std::string_view oldEventId,
                       std::string_view newEventId,
                       std::string_view eventJson,
                       std::string_view orderEntryValue);

void
putEventOrderMapping(Transaction &txn,
                     Store &eventOrderDb,
                     Store &eventToOrderDb,
                     std::uint64_t eventOrder,
                     std::string_view eventId,
                     std::string_view orderEntryValue,
                     PutFlags eventOrderPutFlags = PutFlags::None);

void
putOrderEntry(Transaction &txn,
              Store &eventOrderDb,
              std::uint64_t eventOrder,
              std::string_view eventId,
              std::optional<std::string_view> prevBatch = std::nullopt,
              PutFlags eventOrderPutFlags               = PutFlags::None);

void
putEventOrderMappingForEvent(Transaction &txn,
                             Store &eventOrderDb,
                             Store &eventToOrderDb,
                             std::uint64_t eventOrder,
                             std::string_view eventId,
                             std::optional<std::string_view> prevBatch = std::nullopt,
                             PutFlags eventOrderPutFlags               = PutFlags::None);

void
putMessageOrderMapping(Transaction &txn,
                       Store &orderToMessageDb,
                       Store &messageToOrderDb,
                       std::uint64_t messageOrder,
                       std::string_view eventId,
                       PutFlags orderToMessagePutFlags = PutFlags::None);

std::uint64_t
appendEventOrderEntry(Transaction &txn,
                      Store &eventOrderDb,
                      Store &eventToOrderDb,
                      std::uint64_t &lastEventOrder,
                      std::string_view eventId,
                      std::string_view orderEntryValue);

std::uint64_t
prependEventOrderEntry(Transaction &txn,
                       Store &eventOrderDb,
                       Store &eventToOrderDb,
                       std::uint64_t &firstEventOrder,
                       std::string_view eventId,
                       std::string_view orderEntryValue);

std::uint64_t
appendMessageOrderEntry(Transaction &txn,
                        Store &orderToMessageDb,
                        Store &messageToOrderDb,
                        std::uint64_t &lastMessageOrder,
                        std::string_view eventId);

std::uint64_t
prependMessageOrderEntry(Transaction &txn,
                         Store &orderToMessageDb,
                         Store &messageToOrderDb,
                         std::uint64_t &firstMessageOrder,
                         std::string_view eventId);

std::optional<std::pair<std::uint64_t, std::string>>
lastInvisibleEventAfter(Transaction &txn,
                        Store &eventToOrderDb,
                        Store &eventOrderDb,
                        Store &messageToOrderDb,
                        std::string_view eventId);

std::optional<std::pair<std::uint64_t, std::string>>
lastVisibleEvent(Transaction &txn,
                 Store &eventToOrderDb,
                 Store &eventOrderDb,
                 Store &messageToOrderDb,
                 std::string_view eventId);

std::optional<std::string>
lastTimelineEventId(Transaction &txn, Store &orderToMessageDb);

std::optional<std::pair<std::uint64_t, std::uint64_t>>
timelineRange(Transaction &txn, Store &orderToMessageDb);

std::optional<std::uint64_t>
timelineIndexForEvent(Transaction &txn, Store &messageToOrderDb, std::string_view eventId);

std::optional<std::uint64_t>
eventIndexForEvent(Transaction &txn, Store &eventToOrderDb, std::string_view eventId);

std::optional<std::string>
timelineEventIdAtIndex(Transaction &txn, Store &orderToMessageDb, std::uint64_t index);

std::optional<std::uint64_t>
firstOrderedIndex(Transaction &txn, Store &orderedDb);

std::optional<std::uint64_t>
lastOrderedIndex(Transaction &txn, Store &orderedDb);

std::optional<std::string>
firstPrevBatchToken(Transaction &txn, Store &eventOrderDb);

bool
setOrderEntryPrevBatch(Transaction &txn,
                       Store &eventOrderDb,
                       std::uint64_t eventOrder,
                       std::string_view prevBatch);

std::size_t
removePendingEntriesByTxnId(Transaction &txn, Store &pendingDb, std::string_view txnId);

std::size_t
removeRelationSourceReferences(Transaction &txn,
                               Store &relationsDb,
                               std::string_view sourceEventId);

std::size_t
rewriteRelationSourceReferences(Transaction &txn,
                                Store &relationsDb,
                                std::string_view sourceEventId,
                                std::span<const std::string_view> targetEventIds);

std::vector<std::pair<std::string, std::string>>
listOrderEntriesAfterPrevBatchMarker(Transaction &txn, Store &eventOrderDb);

std::vector<std::string>
listOrderEntryEventIds(Transaction &txn, Store &eventOrderDb);

std::size_t
removeMessageOrderMappingsNotInOrderEntries(Transaction &txn,
                                            Store &eventOrderDb,
                                            Store &orderToMessageDb,
                                            Store &messageToOrderDb);

void
removeOrderEntryReferences(Transaction &txn,
                           Store &eventsDb,
                           Store &relationsDb,
                           Store &eventToOrderDb,
                           Store &messageToOrderDb,
                           Store &orderToMessageDb,
                           std::string_view orderEntryValue);

void
removeOrderEntryWithReferences(Transaction &txn,
                               Store &eventOrderDb,
                               Store &eventsDb,
                               Store &relationsDb,
                               Store &eventToOrderDb,
                               Store &messageToOrderDb,
                               Store &orderToMessageDb,
                               std::string_view orderKey,
                               std::string_view orderEntryValue);

std::size_t
eraseOrderEntriesWithReferencesIf(
  Transaction &txn,
  Store &eventOrderDb,
  Store &eventsDb,
  Store &relationsDb,
  Store &eventToOrderDb,
  Store &messageToOrderDb,
  Store &orderToMessageDb,
  std::size_t startIndex,
  std::size_t limit,
  const std::function<bool(std::string_view orderKey, std::string_view orderEntryValue)>
    &predicate);

std::size_t
trimOldestOrderEntriesWithReferences(Transaction &txn,
                                     Store &eventOrderDb,
                                     Store &eventsDb,
                                     Store &relationsDb,
                                     Store &eventToOrderDb,
                                     Store &messageToOrderDb,
                                     Store &orderToMessageDb,
                                     std::size_t count);

void
cleanupTimelineBeforePrevBatchMarker(Transaction &txn,
                                     Store &eventOrderDb,
                                     Store &eventsDb,
                                     Store &relationsDb,
                                     Store &eventToOrderDb,
                                     Store &messageToOrderDb,
                                     Store &orderToMessageDb);

void
removeTimelineEventReferences(Transaction &txn,
                              Store &eventsDb,
                              Store &relationsDb,
                              Store &eventToOrderDb,
                              Store &messageToOrderDb,
                              Store &orderToMessageDb,
                              std::string_view eventId);

} // namespace db
