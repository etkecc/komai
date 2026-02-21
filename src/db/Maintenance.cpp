// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "db/Maintenance.h"

#include <string_view>

#include "db/Schema.h"
#include "db/Scan.h"
#include "db/StorageApi.h"

namespace db::maintenance {

std::span<const catalog::RoomDb>
roomDbsForFullResync() noexcept
{
    return db::roomDbsForFullResync();
}

bool
tryDropNamedStore(Database &database, Transaction &txn, std::string_view dbName, std::string *error) noexcept
{
    return db::tryDropNamedStore(database, txn, dbName, error);
}

void
migrateLegacyOlmShardsV1ToV2(Database &database, Transaction &txn)
{
    db::migrateLegacyOlmShardsV1ToV2(database, txn);
}

bool
migrateLegacyMegolmSessionIndexes(Database &database,
                                 Transaction &txn,
                                 std::string *error) noexcept
{
    return db::migrateLegacyMegolmSessionIndexes(database, txn, error);
}

bool
migrateLegacyStateByKeyToStatesKey(Database &database,
                                  Transaction &txn,
                                  std::string_view roomId,
                                  std::string *error) noexcept
{
    return db::migrateLegacyStateByKeyToStatesKey(database, txn, roomId, error);
}

bool
migrateLegacyOlmShardsV2ToUnified(Database &database, Transaction &txn, Store &olmSessions)
{
    return db::migrateLegacyOlmShardsV2ToUnified(database, txn, olmSessions);
}

bool
supportsCompaction(const Database &database) noexcept
{
    return database.supportsCompaction();
}

bool
supportsCompaction(const Database *database) noexcept
{
    return database ? supportsCompaction(*database) : false;
}

bool
supportsCompaction(std::unique_ptr<Database> &database) noexcept
{
    return supportsCompaction(database.get());
}

bool
supportsCompaction(const std::unique_ptr<Database> &database) noexcept
{
    return supportsCompaction(database.get());
}

void
compact(Database &from, Database &to)
{
    auto fromTxn = storage::beginTransaction(from, nullptr, storage::AccessFlags::ReadOnly);
    auto toTxn   = storage::beginWriteTransaction(to);

    const auto dbNames = storage::listStoreNames(from, fromTxn);
    for (const auto &dbName : dbNames) {
        auto fromDb = db::storage::openNamedStore(from, fromTxn, dbName, false);
        auto toDb   = db::storage::openNamedStore(to, toTxn, dbName, true);

        forEachEntry(
          fromTxn, fromDb, [&toTxn, &toDb](std::string_view key, std::string_view value) {
              toDb.put(toTxn, key, value, db::PutFlags::AppendDup);
              return true;
          });
    }

    toTxn.commit();
}

void
compact(Database *from, Database *to)
{
    if (!from || !to)
        return;

    compact(*from, *to);
}

} // namespace db::maintenance
