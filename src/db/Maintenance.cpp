// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "db/Maintenance.h"

#include <string_view>

#include "db/Error.h"
#include "db/NamePolicy.h"
#include "db/Scan.h"

namespace db::maintenance {

namespace {

void
requireStoreRequirements(const Database &database, const StoreOpenOptions &options)
{
    if (hasFlag(options.flags, StoreFlags::DupSort) &&
        !database.supports(DatabaseCapability::DuplicateKeys))
        throw Error("Database backend does not support duplicate-key stores", ErrorKind::Invalid);
    if (hasFlag(options.flags, StoreFlags::IntegerKey) &&
        !database.supports(DatabaseCapability::IntegerKeys))
        throw Error("Database backend does not support integer-key stores", ErrorKind::Invalid);
}

Store
openNamedStore(Database &database, Transaction &txn, std::string_view dbName, bool create = true)
{
    auto options = openOptionsForName(dbName);
    if (create)
        options.flags |= StoreFlags::Create;

    requireStoreRequirements(database, options);
    return database.openStore(txn, dbName, options);
}

} // namespace

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
    auto fromTxn = from.beginTxn(nullptr, AccessFlags::ReadOnly);
    auto toTxn   = to.beginTxn();

    const auto dbNames = from.listStoreNames(fromTxn);
    for (const auto &dbName : dbNames) {
        auto fromDb = openNamedStore(from, fromTxn, dbName, false);
        auto toDb   = openNamedStore(to, toTxn, dbName, true);

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
