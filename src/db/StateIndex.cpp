// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "db/StateIndex.h"

#include "db/Catalog.h"
#include "db/DbTypes.h"

namespace db {

std::optional<std::string>
findStateEventId(Txn &txn, Dbi &statesKeyDb, std::string_view eventType, std::string_view stateKey)
{
    auto cursor = Cursor::open(txn, statesKeyDb);

    std::string_view cursorKey = eventType;
    std::string_view value;
    bool first = true;

    if (!cursor.get(cursorKey, value, CursorOp::Set))
        return std::nullopt;

    while (cursor.get(cursorKey, value, first ? CursorOp::FirstDup : CursorOp::NextDup)) {
        first = false;

        const auto [candidateStateKey, candidateEventId] =
          catalog::splitStateEventIndexValue(value);
        if (candidateStateKey == stateKey && !candidateEventId.empty())
            return std::string(candidateEventId);
    }

    return std::nullopt;
}

std::vector<std::string>
listStateEventIds(Txn &txn, Dbi &statesKeyDb, std::string_view eventType)
{
    std::vector<std::string> eventIds;
    auto cursor = Cursor::open(txn, statesKeyDb);

    std::string_view cursorKey = eventType;
    std::string_view value;
    bool first = true;

    if (!cursor.get(cursorKey, value, CursorOp::Set))
        return eventIds;

    while (cursor.get(cursorKey, value, first ? CursorOp::FirstDup : CursorOp::NextDup)) {
        first = false;

        const auto eventId = catalog::splitStateEventIndexValue(value).second;
        if (!eventId.empty())
            eventIds.emplace_back(eventId);
    }

    return eventIds;
}

bool
removeStateEventId(Txn &txn,
                   Dbi &statesKeyDb,
                   std::string_view eventType,
                   std::string_view stateKey,
                   std::string_view eventId)
{
    return statesKeyDb.del(txn, eventType, catalog::stateEventIndexValue(stateKey, eventId));
}

void
putStateEventId(Txn &txn,
                Dbi &statesKeyDb,
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
