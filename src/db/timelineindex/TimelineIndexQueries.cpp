// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "db/TimelineIndex.h"

#include "db/DbTypes.h"
#include "db/OrderEntry.h"
#include "db/Scan.h"

namespace db {

std::optional<std::pair<std::uint64_t, std::string>>
lastInvisibleEventAfter(Txn &txn,
                        Store &eventToOrderDb,
                        Store &eventOrderDb,
                        Store &messageToOrderDb,
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
                 Store &eventToOrderDb,
                 Store &eventOrderDb,
                 Store &messageToOrderDb,
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
lastTimelineEventId(Txn &txn, Store &orderToMessageDb)
{
    const auto last = lastEntry(txn, orderToMessageDb);
    if (!last)
        return std::nullopt;

    return last->second;
}

std::optional<std::pair<std::uint64_t, std::uint64_t>>
timelineRange(Txn &txn, Store &orderToMessageDb)
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
timelineIndexForEvent(Txn &txn, Store &messageToOrderDb, std::string_view eventId)
{
    if (eventId.empty())
        return std::nullopt;

    std::string_view value;
    if (!messageToOrderDb.get(txn, eventId, value))
        return std::nullopt;

    return fromSv<std::uint64_t>(value);
}

std::optional<std::uint64_t>
eventIndexForEvent(Txn &txn, Store &eventToOrderDb, std::string_view eventId)
{
    if (eventId.empty())
        return std::nullopt;

    std::string_view value;
    if (!eventToOrderDb.get(txn, eventId, value))
        return std::nullopt;

    return fromSv<std::uint64_t>(value);
}

std::optional<std::string>
timelineEventIdAtIndex(Txn &txn, Store &orderToMessageDb, std::uint64_t index)
{
    std::string_view value;
    if (!orderToMessageDb.get(txn, toSv(index), value))
        return std::nullopt;

    return std::string(value);
}

std::optional<std::uint64_t>
firstOrderedIndex(Txn &txn, Store &orderedDb)
{
    const auto first = firstEntry(txn, orderedDb);
    if (!first)
        return std::nullopt;

    return fromSv<std::uint64_t>(first->first);
}

std::optional<std::uint64_t>
lastOrderedIndex(Txn &txn, Store &orderedDb)
{
    const auto last = lastEntry(txn, orderedDb);
    if (!last)
        return std::nullopt;

    return fromSv<std::uint64_t>(last->first);
}

std::optional<std::string>
firstPrevBatchToken(Txn &txn, Store &eventOrderDb)
{
    const auto first = firstEntry(txn, eventOrderDb);
    if (!first)
        return std::nullopt;

    return parseOrderEntry(first->second).prevBatch;
}

std::vector<std::pair<std::string, std::string>>
listOrderEntriesAfterPrevBatchMarker(Txn &txn, Store &eventOrderDb)
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
listOrderEntryEventIds(Txn &txn, Store &eventOrderDb)
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

} // namespace db
