// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string_view>

#include "db/Backend.h"
#include "db/Open.h"
#include "db/Catalog.h"

namespace db::storage {

using Database    = db::Database;
using Transaction = db::Transaction;
using Store       = db::Store;
using Cursor      = db::CursorHandle;
using Options     = db::DbiOpenOptions;

enum class AccessMode
{
    ReadWrite,
    ReadOnly,
};

enum class Capability
{
    None,
    Transactions,
};

inline AccessFlags
toAccessFlags(AccessMode mode) noexcept
{
    return mode == AccessMode::ReadOnly ? AccessFlags::ReadOnly : AccessFlags::None;
}

inline Transaction
beginTransaction(Database &database, Transaction *parent = nullptr, AccessMode mode = AccessMode::ReadWrite)
{
    return database.beginTxn(parent, toAccessFlags(mode));
}

inline Transaction
beginReadTransaction(Database &database, Transaction *parent = nullptr)
{
    return beginTransaction(database, parent, AccessMode::ReadOnly);
}

inline Transaction
beginWriteTransaction(Database &database, Transaction *parent = nullptr)
{
    return beginTransaction(database, parent, AccessMode::ReadWrite);
}

inline Cursor
openCursor(Transaction &txn, Store &store)
{
    return Cursor::open(txn, store);
}

inline Store
openGlobalStore(Database &database, Transaction &txn, catalog::GlobalDb store, bool create = true)
{
    return db::openGlobalStore(database, txn, store, create);
}

inline Store
openRoomStore(Database &database,
              Transaction &txn,
              std::string_view roomId,
              catalog::RoomDb store,
              bool create = true)
{
    return db::openRoomStore(database, txn, roomId, store, create);
}

inline Store
openNamedStore(Database &database,
               Transaction &txn,
               std::string_view name,
               bool create = true,
               StoreFlags flags = StoreFlags::None)
{
    Options options;
    options.flags = flags;
    if (create)
        options.flags |= DbiFlags::Create;

    return database.openDbi(txn, name, options);
}

inline Store
openStore(Database &database,
          Transaction &txn,
          std::string_view name,
          bool create = true,
          StoreFlags flags = StoreFlags::None)
{
    return openNamedStore(database, txn, name, create, flags);
}

inline Store
openStore(Database &database, Transaction &txn, std::string_view name, const Options &options)
{
    return database.openDbi(txn, name, options);
}

inline bool
supportsCapability(const Database &database, Capability capability) noexcept
{
    (void) database;
    switch (capability) {
    case Capability::None:
        return true;
    case Capability::Transactions:
        // All backends we expose are transaction-capable today.
        return true;
    default:
        return false;
    }
}

} // namespace db::storage
