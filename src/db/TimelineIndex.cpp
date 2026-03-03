// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "db/TimelineIndex.h"

#include "db/DbTypes.h"
#include "db/OrderEntry.h"
#include "db/Scan.h"

namespace db {

bool
removeMessageOrderMapping(Txn &txn,
                          Store &messageToOrderDb,
                          Store &orderToMessageDb,
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
                       Store &eventsDb,
                       Store &eventOrderDb,
                       Store &eventToOrderDb,
                       Store &messageToOrderDb,
                       Store &orderToMessageDb,
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
                     Store &eventOrderDb,
                     Store &eventToOrderDb,
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
              Store &eventOrderDb,
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
                             Store &eventOrderDb,
                             Store &eventToOrderDb,
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
                       Store &orderToMessageDb,
                       Store &messageToOrderDb,
                       std::uint64_t messageOrder,
                       std::string_view eventId,
                       PutFlags orderToMessagePutFlags)
{
    orderToMessageDb.put(txn, toSv(messageOrder), eventId, orderToMessagePutFlags);
    messageToOrderDb.put(txn, eventId, toSv(messageOrder));
}

std::uint64_t
appendEventOrderEntry(Txn &txn,
                      Store &eventOrderDb,
                      Store &eventToOrderDb,
                      std::uint64_t &lastEventOrder,
                      std::string_view eventId,
                      std::string_view orderEntryValue)
{
    lastEventOrder += 1;
    putEventOrderMapping(txn,
                         eventOrderDb,
                         eventToOrderDb,
                         lastEventOrder,
                         eventId,
                         orderEntryValue,
                         PutFlags::Append);
    return lastEventOrder;
}

std::uint64_t
prependEventOrderEntry(Txn &txn,
                       Store &eventOrderDb,
                       Store &eventToOrderDb,
                       std::uint64_t &firstEventOrder,
                       std::string_view eventId,
                       std::string_view orderEntryValue)
{
    firstEventOrder -= 1;
    putEventOrderMapping(
      txn, eventOrderDb, eventToOrderDb, firstEventOrder, eventId, orderEntryValue);
    return firstEventOrder;
}

std::uint64_t
appendMessageOrderEntry(Txn &txn,
                        Store &orderToMessageDb,
                        Store &messageToOrderDb,
                        std::uint64_t &lastMessageOrder,
                        std::string_view eventId)
{
    lastMessageOrder += 1;
    putMessageOrderMapping(
      txn, orderToMessageDb, messageToOrderDb, lastMessageOrder, eventId, PutFlags::Append);
    return lastMessageOrder;
}

std::uint64_t
prependMessageOrderEntry(Txn &txn,
                         Store &orderToMessageDb,
                         Store &messageToOrderDb,
                         std::uint64_t &firstMessageOrder,
                         std::string_view eventId)
{
    firstMessageOrder -= 1;
    putMessageOrderMapping(txn, orderToMessageDb, messageToOrderDb, firstMessageOrder, eventId);
    return firstMessageOrder;
}

bool
setOrderEntryPrevBatch(Txn &txn,
                       Store &eventOrderDb,
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
removePendingEntriesByTxnId(Txn &txn, Store &pendingDb, std::string_view txnId)
{
    return eraseEntriesIf(
      txn, pendingDb, [txnId](std::string_view /*timestamp*/, std::string_view pendingTxn) {
          return pendingTxn == txnId;
      });
}

} // namespace db
