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

using Transaction = Txn;
using Store       = Dbi;

std::optional<std::string>
findStateEventId(Transaction &txn,
                 Store &statesKeyDb,
                 std::string_view eventType,
                 std::string_view stateKey);

std::vector<std::string>
listStateEventIds(Transaction &txn, Store &statesKeyDb, std::string_view eventType);

bool
removeStateEventId(Transaction &txn,
                   Store &statesKeyDb,
                   std::string_view eventType,
                   std::string_view stateKey,
                   std::string_view eventId);

void
putStateEventId(Transaction &txn,
                Store &statesKeyDb,
                std::string_view eventType,
                std::string_view stateKey,
                std::string_view eventId);

} // namespace db
