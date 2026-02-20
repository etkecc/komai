// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace db {

class Txn;
class Dbi;

std::optional<std::string>
findStateEventId(Txn &txn, Dbi &statesKeyDb, std::string_view eventType, std::string_view stateKey);

std::vector<std::string>
listStateEventIds(Txn &txn, Dbi &statesKeyDb, std::string_view eventType);

bool
removeStateEventId(Txn &txn,
                   Dbi &statesKeyDb,
                   std::string_view eventType,
                   std::string_view stateKey,
                   std::string_view eventId);

void
putStateEventId(Txn &txn,
                Dbi &statesKeyDb,
                std::string_view eventType,
                std::string_view stateKey,
                std::string_view eventId);

} // namespace db
