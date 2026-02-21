// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>
#include <string_view>
#include <memory>
#include <vector>

#include "db/Error.h"
#include "db/Backend.h"
#include "db/NamePolicy.h"
#include "db/Catalog.h"

namespace db::storage {

using Database    = db::Database;
using Transaction = db::Transaction;
using Store       = db::Store;
using CursorHandle = db::CursorHandle;
using Options     = db::StoreOpenOptions;
using DatabaseOptions = db::BackendOptions;

enum class AccessMode
{
    ReadWrite,
    ReadOnly,
};

enum class Capability
{
    None,
    Transactions,
    DuplicateKeys,
    IntegerKeys,
    PrefixScan,
};

inline AccessFlags
toAccessFlags(AccessMode mode) noexcept
{
    return mode == AccessMode::ReadOnly ? AccessFlags::ReadOnly : AccessFlags::None;
}

inline bool
supportsCapability(const Database &database, Capability capability) noexcept
{
    switch (capability) {
    case Capability::None:
        return true;
    case Capability::Transactions:
        // All backends we expose are transaction-capable today.
        return true;
    case Capability::DuplicateKeys:
        return database.supports(db::StoreCapability::DuplicateKeys);
    case Capability::IntegerKeys:
        return database.supports(db::StoreCapability::IntegerKeys);
    case Capability::PrefixScan:
        return database.supports(db::StoreCapability::PrefixScan);
    default:
        return false;
    }
}

inline void
requireCapabilities(const Database &database, StoreFlags flags)
{
    if (hasFlag(flags, StoreFlags::DupSort) && !supportsCapability(database, Capability::DuplicateKeys))
        throw Error("Backend does not support duplicate-key stores", ErrorKind::Invalid);
    if (hasFlag(flags, StoreFlags::IntegerKey) &&
        !supportsCapability(database, Capability::IntegerKeys))
        throw Error("Backend does not support integer-key stores", ErrorKind::Invalid);
}

inline void
open(Database &database, std::string_view directory, const DatabaseOptions &options = {})
{
    database.open(directory, options);
}

inline void
open(Database *database, std::string_view directory, const DatabaseOptions &options = {})
{
    if (!database)
        throw Error("Database pointer is null", ErrorKind::Invalid);

    open(*database, directory, options);
}

inline void
open(std::unique_ptr<Database> &database,
     std::string_view directory,
     const DatabaseOptions &options = {})
{
    open(database.get(), directory, options);
}

inline void
open(const std::unique_ptr<Database> &database, std::string_view directory, const DatabaseOptions &options = {})
{
    open(database.get(), directory, options);
}

inline void
close(Database &database)
{
    database.close();
}

inline void
close(Database *database)
{
    if (!database)
        throw Error("Database pointer is null", ErrorKind::Invalid);

    close(*database);
}

inline void
close(std::unique_ptr<Database> &database)
{
    close(database.get());
}

inline void
close(const std::unique_ptr<Database> &database)
{
    close(database.get());
}

inline bool
isOpen(const Database &database)
{
    return database.isOpen();
}

inline bool
isOpen(const Database *database)
{
    return database ? isOpen(*database) : false;
}

inline bool
isOpen(const std::unique_ptr<Database> &database)
{
    return isOpen(database.get());
}

inline bool
isOpen(const std::unique_ptr<const Database> &database)
{
    return isOpen(database.get());
}

inline std::string_view
id(const Database &database)
{
    return database.id();
}

inline std::string_view
id(Database *database)
{
    if (!database)
        throw Error("Database pointer is null", ErrorKind::Invalid);

    return id(*database);
}

inline std::string_view
id(std::unique_ptr<Database> &database)
{
    return id(database.get());
}

inline std::string_view
id(const std::unique_ptr<Database> &database)
{
    return id(database.get());
}

inline bool
supportsCompaction(const Database &database)
{
    return database.supportsCompaction();
}

inline bool
supportsCompaction(Database *database)
{
    return database ? supportsCompaction(*database) : false;
}

inline bool
supportsCompaction(std::unique_ptr<Database> &database)
{
    return supportsCompaction(database.get());
}

inline bool
supportsCompaction(const std::unique_ptr<Database> &database)
{
    return supportsCompaction(database.get());
}

inline StorageCategory
storageCategory(const Database &database)
{
    return database.storageCategory();
}

inline StorageCategory
storageCategory(Database *database)
{
    if (!database)
        throw Error("Database pointer is null", ErrorKind::Invalid);

    return storageCategory(*database);
}

inline StorageCategory
storageCategory(std::unique_ptr<Database> &database)
{
    return storageCategory(database.get());
}

inline StorageCategory
storageCategory(const std::unique_ptr<Database> &database)
{
    return storageCategory(database.get());
}

class Cursor
{
public:
    Cursor() = default;
    Cursor(Transaction &txn, Store &store)
      : handle_(db::Cursor::open(txn, store))
    {}

    static Cursor
    open(Transaction &txn, Store &store)
    {
        return Cursor{txn, store};
    }

    bool
    moveFirst(std::string &key, std::string &value)
    {
        return move(MoveOp::First, key, value);
    }

    bool
    moveLast(std::string &key, std::string &value)
    {
        return move(MoveOp::Last, key, value);
    }

    bool
    moveNext(std::string &key, std::string &value)
    {
        return move(MoveOp::Next, key, value);
    }

    bool
    movePrev(std::string &key, std::string &value)
    {
        return move(MoveOp::Prev, key, value);
    }

    bool
    moveNextNoDup(std::string &key, std::string &value)
    {
        return move(MoveOp::NextNoDup, key, value);
    }

    bool
    moveNextDup(std::string &key, std::string &value)
    {
        return move(MoveOp::NextDup, key, value);
    }

    bool
    moveTo(std::string_view key, std::string &foundKey, std::string &foundValue)
    {
        std::string seekKey{key};
        return move(MoveOp::Set, seekKey, foundKey, foundValue);
    }

    bool
    moveToRange(std::string_view key, std::string &foundKey, std::string &foundValue)
    {
        std::string seekKey{key};
        return move(MoveOp::SetRange, seekKey, foundKey, foundValue);
    }

private:
    bool
    move(MoveOp op, std::string &key, std::string &value)
    {
        std::string_view keyBytes = key;
        std::string_view valueBytes;

        if (!handle_.get(keyBytes, valueBytes, op))
            return false;

        key.assign(keyBytes.data(), keyBytes.size());
        value.assign(valueBytes.data(), valueBytes.size());
        return true;
    }

    bool
    move(MoveOp op, std::string &seek, std::string &key, std::string &value)
    {
        key = seek;
        return move(op, key, value);
    }

    CursorHandle handle_;
};

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

inline Transaction
beginTransaction(Database &database, Transaction *parent, TxnFlags flags)
{
    return database.beginTxn(parent, flags);
}

inline Cursor
openCursor(Transaction &txn, Store &store)
{
    return Cursor::open(txn, store);
}

inline std::vector<std::string>
listStoreNames(Database &database, Transaction &txn)
{
    return database.listStoreNames(txn);
}

inline Store
openNamedStore(Database &database,
               Transaction &txn,
               std::string_view name,
               bool create = true,
               StoreFlags flags = StoreFlags::None)
{
    auto options      = openOptionsForName(name);
    options.flags   |= flags;
    if (create)
        options.flags |= StoreFlags::Create;

    requireCapabilities(database, options.flags);
    return database.openStore(txn, name, options);
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
    requireCapabilities(database, options.flags);
    return database.openStore(txn, name, options);
}

inline Store
openGlobalStore(Database &database, Transaction &txn, catalog::GlobalDb store, bool create = true)
{
    auto options = openOptionsForGlobal(store);
    if (create)
        options.flags |= StoreFlags::Create;

    return openStore(database, txn, catalog::globalName(store), options);
}

inline Store
openRoomStore(Database &database,
              Transaction &txn,
              std::string_view roomId,
              catalog::RoomDb store,
              bool create = true)
{
    auto options = openOptionsForRoom(store);
    if (create)
        options.flags |= StoreFlags::Create;

    return openStore(database, txn, catalog::roomName(roomId, store), options);
}

} // namespace db::storage
