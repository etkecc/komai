// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "db/Maintenance.h"

#include <string_view>

#include "db/Schema.h"
#include "db/Compaction.h"

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

void
compact(Database &from, Database &to)
{
    db::compact(from, to);
}

} // namespace db::maintenance
