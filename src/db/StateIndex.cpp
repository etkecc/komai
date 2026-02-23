// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "db/StateIndex.h"

#include <vector>

#include "db/Catalog.h"
#include "db/DbTypes.h"
#include "db/DupIndex.h"

namespace db {

void
removeStateEventIdsForStateKey(Txn &txn,
                               Store &statesKeyDb,
                               std::string_view eventType,
                               std::string_view stateKey)
{
    std::vector<std::string> valuesToRemove;
    forEachDupValue(txn, statesKeyDb, eventType, [&](std::string_view value) {
        const auto [candidateStateKey, candidateEventId] =
          catalog::splitStateEventIndexValue(value);
        if (candidateStateKey == stateKey && !candidateEventId.empty())
            valuesToRemove.push_back(std::string(value));

        return true;
    });

    for (const auto &value : valuesToRemove)
        statesKeyDb.del(txn, eventType, value);
}

std::optional<std::string>
findStateEventId(Txn &txn,
                 Store &statesKeyDb,
                 std::string_view eventType,
                 std::string_view stateKey)
{
    std::optional<std::string> foundEventId;
    forEachDupValue(txn, statesKeyDb, eventType, [&foundEventId, stateKey](std::string_view value) {
        const auto [candidateStateKey, candidateEventId] =
          catalog::splitStateEventIndexValue(value);
        if (candidateStateKey == stateKey && !candidateEventId.empty()) {
            foundEventId = std::string(candidateEventId);
            return false;
        }
        return true;
    });

    return foundEventId;
}

std::vector<std::string>
listStateEventIds(Txn &txn, Store &statesKeyDb, std::string_view eventType)
{
    std::vector<std::string> eventIds;
    forEachDupValue(txn, statesKeyDb, eventType, [&eventIds](std::string_view value) {
        const auto eventId = catalog::splitStateEventIndexValue(value).second;
        if (!eventId.empty())
            eventIds.emplace_back(eventId);
        return true;
    });

    return eventIds;
}

bool
removeStateEventId(Txn &txn,
                   Store &statesKeyDb,
                   std::string_view eventType,
                   std::string_view stateKey,
                   std::string_view eventId)
{
    return statesKeyDb.del(txn, eventType, catalog::stateEventIndexValue(stateKey, eventId));
}

void
putStateEventId(Txn &txn,
                Store &statesKeyDb,
                std::string_view eventType,
                std::string_view stateKey,
                std::string_view eventId)
{
    removeStateEventIdsForStateKey(txn, statesKeyDb, eventType, stateKey);

    auto compositeValue = catalog::stateEventIndexValue(stateKey, eventId);

    statesKeyDb.put(txn, eventType, compositeValue);
}

} // namespace db
