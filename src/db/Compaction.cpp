// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "db/Compaction.h"

#include "db/Open.h"
#include "db/Scan.h"
#include "db/StorageApi.h"

namespace db {

void
compact(Database &from, Database &to)
{
    auto fromTxn = from.beginTxn(nullptr, AccessFlags::ReadOnly);
    auto toTxn   = to.beginTxn();

    const auto dbNames = from.listDbiNames(fromTxn);
    for (const auto &dbName : dbNames) {
        auto fromDb = openNamedStore(from, fromTxn, dbName, false);
        auto toDb   = openNamedStore(to, toTxn, dbName, true);

        forEachEntry(
          fromTxn, fromDb, [&toTxn, &toDb](std::string_view key, std::string_view value) {
              toDb.put(toTxn, key, value, PutFlags::AppendDup);
              return true;
          });
    }

    toTxn.commit();
}

} // namespace db
