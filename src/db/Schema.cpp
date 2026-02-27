// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "db/Schema.h"

#include <exception>

#include "db/Error.h"
#include "db/NamePolicy.h"

namespace db {

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
openNamedStore(Database &database, Transaction &txn, std::string_view dbName, bool create)
{
    auto options = openOptionsForName(dbName);
    if (create)
        options.flags |= StoreFlags::Create;

    requireStoreRequirements(database, options);
    return database.openStore(txn, dbName, options);
}

} // namespace

bool
tryDropNamedStore(Database &database,
                  Transaction &txn,
                  std::string_view dbName,
                  std::string *error) noexcept
{
    if (error)
        error->clear();

    try {
        openNamedStore(database, txn, dbName, false).drop(txn, true);
        return true;
    } catch (const std::exception &e) {
        if (error)
            *error = e.what();
        return false;
    } catch (...) {
        if (error)
            *error = "unknown error";
        return false;
    }
}

} // namespace db
