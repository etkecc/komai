// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "db/Compaction.h"

#include "db/Scan.h"
#include "db/StorageApi.h"

namespace db {

void
compact(Database &from, Database &to)
{
    auto fromTxn = storage::beginTransaction(from, nullptr, AccessFlags::ReadOnly);
    auto toTxn   = storage::beginWriteTransaction(to);

    const auto dbNames = storage::listStoreNames(from, fromTxn);
    for (const auto &dbName : dbNames) {
        auto fromDb = db::storage::openNamedStore(from, fromTxn, dbName, false);
        auto toDb   = db::storage::openNamedStore(to, toTxn, dbName, true);

        forEachEntry(
          fromTxn, fromDb, [&toTxn, &toDb](std::string_view key, std::string_view value) {
              toDb.put(toTxn, key, value, PutFlags::AppendDup);
              return true;
          });
    }

    toTxn.commit();
}

} // namespace db
