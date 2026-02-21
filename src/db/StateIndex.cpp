// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "db/StateIndex.h"

#include "db/Catalog.h"
#include "db/DbTypes.h"
#include "db/DupIndex.h"

namespace db {

std::optional<std::string>
findStateEventId(Txn &txn, Store &statesKeyDb, std::string_view eventType, std::string_view stateKey)
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
    auto compositeValue = catalog::stateEventIndexValue(stateKey, eventId);

    // Work around https://bugs.openldap.org/show_bug.cgi?id=8447
    statesKeyDb.del(txn, eventType, compositeValue);
    statesKeyDb.put(txn, eventType, compositeValue);
}

} // namespace db
