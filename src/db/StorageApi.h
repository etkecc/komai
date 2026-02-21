// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>
#include <string_view>
#include <memory>
#include <optional>
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
using BackendId = std::string_view;

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

inline std::unique_ptr<Database>
createDatabase()
{
    return db::createDefaultBackend();
}

inline std::unique_ptr<Database>
createDatabase(BackendId id)
{
    return db::createBackend(id);
}

inline std::unique_ptr<Database>
createConfiguredDatabase(BackendId requestedId = {})
{
    return db::createConfiguredBackend(requestedId);
}

inline std::unique_ptr<Database>
createDatabaseFromEnvironment(BackendId variableName = "KOMAI_DB_BACKEND")
{
    return db::createConfiguredBackendFromEnvironment(variableName);
}

inline bool
isDatabaseSupported(BackendId id) noexcept
{
    return db::isBackendSupported(id);
}

inline std::string_view
defaultDatabaseId() noexcept
{
    return db::defaultBackendId();
}

inline BackendId
canonicalDatabaseId(BackendId id) noexcept
{
    return db::canonicalBackendId(id);
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

inline std::optional<std::size_t>
mapSizeBytes(const Database &database)
{
    return database.mapSizeBytes();
}

inline std::optional<std::size_t>
mapSizeBytes(Database *database)
{
    return database ? mapSizeBytes(*database) : std::nullopt;
}

inline std::optional<std::size_t>
mapSizeBytes(std::unique_ptr<Database> &database)
{
    return mapSizeBytes(database.get());
}

inline std::optional<std::size_t>
mapSizeBytes(const std::unique_ptr<Database> &database)
{
    return mapSizeBytes(database.get());
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
beginTransaction(Database *database, Transaction *parent = nullptr, AccessMode mode = AccessMode::ReadWrite)
{
    if (!database)
        throw Error("Database pointer is null", ErrorKind::Invalid);

    return beginTransaction(*database, parent, mode);
}

inline Transaction
beginTransaction(std::unique_ptr<Database> &database,
                Transaction *parent = nullptr,
                AccessMode mode = AccessMode::ReadWrite)
{
    return beginTransaction(database.get(), parent, mode);
}

inline Transaction
beginTransaction(const std::unique_ptr<Database> &database,
                Transaction *parent = nullptr,
                AccessMode mode = AccessMode::ReadWrite)
{
    return beginTransaction(database.get(), parent, mode);
}

inline Transaction
beginReadTransaction(Database &database, Transaction *parent = nullptr)
{
    return beginTransaction(database, parent, AccessMode::ReadOnly);
}

inline Transaction
beginReadTransaction(Database *database, Transaction *parent = nullptr)
{
    return beginTransaction(database, parent, AccessMode::ReadOnly);
}

inline Transaction
beginReadTransaction(std::unique_ptr<Database> &database, Transaction *parent = nullptr)
{
    return beginTransaction(database, parent, AccessMode::ReadOnly);
}

inline Transaction
beginReadTransaction(const std::unique_ptr<Database> &database, Transaction *parent = nullptr)
{
    return beginTransaction(database, parent, AccessMode::ReadOnly);
}

inline Transaction
beginWriteTransaction(Database &database, Transaction *parent = nullptr)
{
    return beginTransaction(database, parent, AccessMode::ReadWrite);
}

inline Transaction
beginWriteTransaction(Database *database, Transaction *parent = nullptr)
{
    return beginTransaction(database, parent, AccessMode::ReadWrite);
}

inline Transaction
beginWriteTransaction(std::unique_ptr<Database> &database, Transaction *parent = nullptr)
{
    return beginTransaction(database, parent, AccessMode::ReadWrite);
}

inline Transaction
beginWriteTransaction(const std::unique_ptr<Database> &database, Transaction *parent = nullptr)
{
    return beginTransaction(database, parent, AccessMode::ReadWrite);
}

inline Transaction
beginTransaction(Database &database, Transaction *parent, TxnFlags flags)
{
    return database.beginTxn(parent, flags);
}

inline Transaction
beginTransaction(Database *database, Transaction *parent, TxnFlags flags)
{
    if (!database)
        throw Error("Database pointer is null", ErrorKind::Invalid);

    return beginTransaction(*database, parent, flags);
}

inline Transaction
beginTransaction(std::unique_ptr<Database> &database, Transaction *parent, TxnFlags flags)
{
    return beginTransaction(database.get(), parent, flags);
}

inline Transaction
beginTransaction(const std::unique_ptr<Database> &database, Transaction *parent, TxnFlags flags)
{
    return beginTransaction(database.get(), parent, flags);
}

inline Cursor
openCursor(Transaction &txn, Store &store)
{
    return Cursor::open(txn, store);
}

inline bool
ownsTransaction(const Database &database, const Transaction &transaction)
{
    return database.ownsTxn(transaction);
}

inline bool
ownsTransaction(const Database *database, const Transaction &transaction)
{
    return database ? ownsTransaction(*database, transaction) : false;
}

inline bool
ownsTransaction(const std::unique_ptr<Database> &database, const Transaction &transaction)
{
    return ownsTransaction(database.get(), transaction);
}

inline bool
ownsTransaction(std::unique_ptr<Database> &database, const Transaction &transaction)
{
    return ownsTransaction(database.get(), transaction);
}

inline std::vector<std::string>
listStoreNames(Database &database, Transaction &txn)
{
    return database.listStoreNames(txn);
}

inline std::vector<std::string>
listStoreNames(Database *database, Transaction &txn)
{
    if (!database)
        throw Error("Database pointer is null", ErrorKind::Invalid);

    return listStoreNames(*database, txn);
}

inline std::vector<std::string>
listStoreNames(std::unique_ptr<Database> &database, Transaction &txn)
{
    return listStoreNames(database.get(), txn);
}

inline std::vector<std::string>
listStoreNames(const std::unique_ptr<Database> &database, Transaction &txn)
{
    return listStoreNames(database.get(), txn);
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
openNamedStore(Database *database,
               Transaction &txn,
               std::string_view name,
               bool create = true,
               StoreFlags flags = StoreFlags::None)
{
    if (!database)
        throw Error("Database pointer is null", ErrorKind::Invalid);

    return openNamedStore(*database, txn, name, create, flags);
}

inline Store
openNamedStore(std::unique_ptr<Database> &database,
               Transaction &txn,
               std::string_view name,
               bool create = true,
               StoreFlags flags = StoreFlags::None)
{
    return openNamedStore(database.get(), txn, name, create, flags);
}

inline Store
openNamedStore(const std::unique_ptr<Database> &database,
               Transaction &txn,
               std::string_view name,
               bool create = true,
               StoreFlags flags = StoreFlags::None)
{
    return openNamedStore(database.get(), txn, name, create, flags);
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
openStore(Database *database,
          Transaction &txn,
          std::string_view name,
          bool create = true,
          StoreFlags flags = StoreFlags::None)
{
    if (!database)
        throw Error("Database pointer is null", ErrorKind::Invalid);

    return openNamedStore(database, txn, name, create, flags);
}

inline Store
openStore(std::unique_ptr<Database> &database,
          Transaction &txn,
          std::string_view name,
          bool create = true,
          StoreFlags flags = StoreFlags::None)
{
    return openNamedStore(database, txn, name, create, flags);
}

inline Store
openStore(const std::unique_ptr<Database> &database,
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
openStore(Database *database, Transaction &txn, std::string_view name, const Options &options)
{
    if (!database)
        throw Error("Database pointer is null", ErrorKind::Invalid);

    return openStore(*database, txn, name, options);
}

inline Store
openStore(std::unique_ptr<Database> &database, Transaction &txn, std::string_view name, const Options &options)
{
    return openStore(database.get(), txn, name, options);
}

inline Store
openStore(const std::unique_ptr<Database> &database, Transaction &txn, std::string_view name, const Options &options)
{
    return openStore(database.get(), txn, name, options);
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
openGlobalStore(Database *database, Transaction &txn, catalog::GlobalDb store, bool create = true)
{
    if (!database)
        throw Error("Database pointer is null", ErrorKind::Invalid);

    return openGlobalStore(*database, txn, store, create);
}

inline Store
openGlobalStore(std::unique_ptr<Database> &database, Transaction &txn, catalog::GlobalDb store, bool create = true)
{
    return openGlobalStore(database.get(), txn, store, create);
}

inline Store
openGlobalStore(const std::unique_ptr<Database> &database, Transaction &txn, catalog::GlobalDb store, bool create = true)
{
    return openGlobalStore(database.get(), txn, store, create);
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

inline Store
openRoomStore(Database *database,
              Transaction &txn,
              std::string_view roomId,
              catalog::RoomDb store,
              bool create = true)
{
    if (!database)
        throw Error("Database pointer is null", ErrorKind::Invalid);

    return openRoomStore(*database, txn, roomId, store, create);
}

inline Store
openRoomStore(std::unique_ptr<Database> &database,
              Transaction &txn,
              std::string_view roomId,
              catalog::RoomDb store,
              bool create = true)
{
    return openRoomStore(database.get(), txn, roomId, store, create);
}

inline Store
openRoomStore(const std::unique_ptr<Database> &database,
              Transaction &txn,
              std::string_view roomId,
              catalog::RoomDb store,
              bool create = true)
{
    return openRoomStore(database.get(), txn, roomId, store, create);
}

} // namespace db::storage
