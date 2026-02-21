// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "db/Flags.h"

namespace db {

class Txn;
class Dbi;

bool
removeMessageOrderMapping(Txn &txn,
                          Dbi &messageToOrderDb,
                          Dbi &orderToMessageDb,
                          std::string_view eventId);

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
                       std::string_view orderEntryValue);

void
putEventOrderMapping(Txn &txn,
                     Dbi &eventOrderDb,
                     Dbi &eventToOrderDb,
                     std::uint64_t eventOrder,
                     std::string_view eventId,
                     std::string_view orderEntryValue,
                     PutFlags eventOrderPutFlags = PutFlags::None);

void
putMessageOrderMapping(Txn &txn,
                       Dbi &orderToMessageDb,
                       Dbi &messageToOrderDb,
                       std::uint64_t messageOrder,
                       std::string_view eventId,
                       PutFlags orderToMessagePutFlags = PutFlags::None);

std::optional<std::pair<std::uint64_t, std::string>>
lastInvisibleEventAfter(Txn &txn,
                        Dbi &eventToOrderDb,
                        Dbi &eventOrderDb,
                        Dbi &messageToOrderDb,
                        std::string_view eventId);

std::optional<std::pair<std::uint64_t, std::string>>
lastVisibleEvent(Txn &txn,
                 Dbi &eventToOrderDb,
                 Dbi &eventOrderDb,
                 Dbi &messageToOrderDb,
                 std::string_view eventId);

std::optional<std::string>
lastTimelineEventId(Txn &txn, Dbi &orderToMessageDb);

std::optional<std::pair<std::uint64_t, std::uint64_t>>
timelineRange(Txn &txn, Dbi &orderToMessageDb);

std::optional<std::uint64_t>
timelineIndexForEvent(Txn &txn, Dbi &messageToOrderDb, std::string_view eventId);

std::optional<std::uint64_t>
eventIndexForEvent(Txn &txn, Dbi &eventToOrderDb, std::string_view eventId);

std::optional<std::string>
timelineEventIdAtIndex(Txn &txn, Dbi &orderToMessageDb, std::uint64_t index);

std::optional<std::uint64_t>
firstOrderedIndex(Txn &txn, Dbi &orderedDb);

std::optional<std::uint64_t>
lastOrderedIndex(Txn &txn, Dbi &orderedDb);

std::optional<std::string>
firstPrevBatchToken(Txn &txn, Dbi &eventOrderDb);

bool
setOrderEntryPrevBatch(Txn &txn,
                       Dbi &eventOrderDb,
                       std::uint64_t eventOrder,
                       std::string_view prevBatch);

std::size_t
removePendingEntriesByTxnId(Txn &txn, Dbi &pendingDb, std::string_view txnId);

void
removeTimelineEventReferences(Txn &txn,
                              Dbi &eventsDb,
                              Dbi &relationsDb,
                              Dbi &eventToOrderDb,
                              Dbi &messageToOrderDb,
                              Dbi &orderToMessageDb,
                              std::string_view eventId);

} // namespace db
