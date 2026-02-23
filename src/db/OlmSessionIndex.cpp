// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "db/OlmSessionIndex.h"

#include "db/Catalog.h"
#include "db/DbTypes.h"
#include "db/Scan.h"

namespace db {

bool
getOlmSessionValue(Txn &txn,
                   Store &olmSessionsDb,
                   std::string_view curve25519,
                   std::string_view sessionId,
                   std::string_view &value)
{
    return olmSessionsDb.get(txn, catalog::olmSessionKey(curve25519, sessionId), value);
}

void
putOlmSessionValue(Txn &txn,
                   Store &olmSessionsDb,
                   std::string_view curve25519,
                   std::string_view sessionId,
                   std::string_view value)
{
    olmSessionsDb.put(txn, catalog::olmSessionKey(curve25519, sessionId), value);
}

std::size_t
forEachOlmSessionForCurve(
  Txn &txn,
  Store &olmSessionsDb,
  std::string_view curve25519,
  const std::function<bool(std::string_view sessionId, std::string_view value)> &callback)
{
    std::size_t count = 0;
    const auto prefix = catalog::olmSessionKey(curve25519, "");
    forEachEntryWithPrefix(txn,
                           olmSessionsDb,
                           prefix,
                           [&callback, &count](std::string_view key, std::string_view value) {
                               ++count;
                               return callback(catalog::splitOlmSessionKey(key).second, value);
                           });

    return count;
}

std::vector<std::string>
listOlmSessionIds(Txn &txn, Store &olmSessionsDb, std::string_view curve25519)
{
    std::vector<std::string> sessionIds;
    forEachOlmSessionForCurve(
      txn,
      olmSessionsDb,
      curve25519,
      [&sessionIds](std::string_view sessionId, std::string_view /*value*/) {
          sessionIds.emplace_back(sessionId);
          return true;
      });
    return sessionIds;
}

} // namespace db
