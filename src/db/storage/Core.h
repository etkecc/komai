// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "db/Backend.h"
#include "db/Error.h"

namespace db::storage {

using Database         = db::Database;
using Transaction      = db::Transaction;
using Store            = db::Store;
using Cursor           = db::Cursor;
using CursorHandle     = db::CursorHandle;
using Options          = db::StoreOpenOptions;
using StoreOpenOptions = db::StoreOpenOptions;
using DatabaseOptions  = db::DatabaseOptions;
using DatabaseId       = db::DatabaseId;
using StorageCategory  = db::StorageCategory;
using Error            = db::Error;
using ErrorKind        = db::ErrorKind;
using AccessFlags      = db::AccessFlags;
using TransactionFlags = db::TxnFlags;
using StoreFlags       = db::StoreFlags;
using BackendId        = DatabaseId;
using DatabaseIdSet    = db::DatabaseIdSet;
using StoreCapability  = db::StoreCapability;

inline constexpr std::string_view kMemoryDatabaseId   = db::kMemoryDatabaseId;
inline constexpr std::string_view kInMemoryDatabaseId = db::kInMemoryDatabaseId;
inline constexpr std::string_view kLmdbDatabaseId     = db::kLmdbDatabaseId;
inline constexpr std::string_view kMemoryBackendId    = db::kMemoryBackendId;
inline constexpr std::string_view kInMemoryBackendId  = db::kInMemoryBackendId;
inline constexpr std::string_view kLmdbBackendId      = db::kLmdbBackendId;

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
    if (hasFlag(flags, StoreFlags::DupSort) &&
        !supportsCapability(database, Capability::DuplicateKeys))
        throw Error("Database backend does not support duplicate-key stores", ErrorKind::Invalid);
    if (hasFlag(flags, StoreFlags::IntegerKey) &&
        !supportsCapability(database, Capability::IntegerKeys))
        throw Error("Database backend does not support integer-key stores", ErrorKind::Invalid);
}

inline std::unique_ptr<Database>
createDatabase()
{
    return db::createDefaultDatabase();
}

inline std::unique_ptr<Database>
createDatabase(DatabaseId id)
{
    return db::createDatabase(id);
}

inline std::unique_ptr<Database>
createConfiguredDatabase(DatabaseId requestedId = {})
{
    return db::createConfiguredDatabase(requestedId);
}

inline std::unique_ptr<Database>
createDatabaseFromEnvironment(DatabaseId variableName = "KOMAI_DB_BACKEND")
{
    return db::createConfiguredDatabaseFromEnvironment(variableName);
}

inline bool
isDatabaseSupported(DatabaseId id) noexcept
{
    return db::isDatabaseSupported(id);
}

inline bool
isBackendSupported(DatabaseId id) noexcept
{
    return db::isDatabaseSupported(id);
}

inline DatabaseId
defaultDatabaseId() noexcept
{
    return db::defaultDatabaseId();
}

inline DatabaseId
defaultBackendId() noexcept
{
    return defaultDatabaseId();
}

inline DatabaseId
canonicalBackendId(DatabaseId id) noexcept
{
    return canonicalDatabaseId(id);
}

inline DatabaseId
canonicalDatabaseId(DatabaseId id) noexcept
{
    return db::canonicalDatabaseId(id);
}

inline std::span<const DatabaseId>
availableDatabaseIds()
{
    return db::availableDatabaseIds();
}

inline std::span<const DatabaseId>
availableBackendIds() noexcept
{
    return availableDatabaseIds();
}

inline std::unique_ptr<Database>
createBackend(DatabaseId id)
{
    return createDatabase(id);
}

inline std::unique_ptr<Database>
createConfiguredBackend(DatabaseId requestedId = {})
{
    return createConfiguredDatabase(requestedId);
}

inline std::unique_ptr<Database>
createConfiguredBackendFromEnvironment(DatabaseId variableName = "KOMAI_DB_BACKEND")
{
    return createDatabaseFromEnvironment(variableName);
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
open(const std::unique_ptr<Database> &database,
     std::string_view directory,
     const DatabaseOptions &options = {})
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
    return database ? ::db::isOpen(*database) : false;
}

inline bool
isOpen(const std::unique_ptr<Database> &database)
{
    return ::db::isOpen(database.get());
}

inline bool
isOpen(const std::unique_ptr<const Database> &database)
{
    return ::db::isOpen(database.get());
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

    return ::db::id(*database);
}

inline std::string_view
id(std::unique_ptr<Database> &database)
{
    return ::db::id(database.get());
}

inline std::string_view
id(const std::unique_ptr<Database> &database)
{
    return ::db::id(database.get());
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
    return database ? ::db::mapSizeBytes(*database) : std::nullopt;
}

inline std::optional<std::size_t>
mapSizeBytes(std::unique_ptr<Database> &database)
{
    return ::db::mapSizeBytes(database.get());
}

inline std::optional<std::size_t>
mapSizeBytes(const std::unique_ptr<Database> &database)
{
    return ::db::mapSizeBytes(database.get());
}

inline StorageCategory
storageCategory(Database *database)
{
    if (!database)
        throw Error("Database pointer is null", ErrorKind::Invalid);

    return ::db::storageCategory(*database);
}

inline StorageCategory
storageCategory(std::unique_ptr<Database> &database)
{
    return ::db::storageCategory(database.get());
}

inline StorageCategory
storageCategory(const std::unique_ptr<Database> &database)
{
    return ::db::storageCategory(database.get());
}

inline Transaction
beginTransaction(Database &database,
                 Transaction *parent = nullptr,
                 AccessMode mode     = AccessMode::ReadWrite)
{
    return database.beginTxn(parent, toAccessFlags(mode));
}

inline Transaction
beginTransaction(Database *database,
                 Transaction *parent = nullptr,
                 AccessMode mode     = AccessMode::ReadWrite)
{
    if (!database)
        throw Error("Database pointer is null", ErrorKind::Invalid);

    return beginTransaction(*database, parent, mode);
}

inline Transaction
beginTransaction(std::unique_ptr<Database> &database,
                 Transaction *parent = nullptr,
                 AccessMode mode     = AccessMode::ReadWrite)
{
    return beginTransaction(database.get(), parent, mode);
}

inline Transaction
beginTransaction(const std::unique_ptr<Database> &database,
                 Transaction *parent = nullptr,
                 AccessMode mode     = AccessMode::ReadWrite)
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
beginTransaction(Database &database, Transaction *parent, TransactionFlags flags)
{
    return database.beginTxn(parent, flags);
}

inline Transaction
beginTransaction(Database *database, Transaction *parent, TransactionFlags flags)
{
    if (!database)
        throw Error("Database pointer is null", ErrorKind::Invalid);

    return beginTransaction(*database, parent, flags);
}

inline Transaction
beginTransaction(std::unique_ptr<Database> &database, Transaction *parent, TransactionFlags flags)
{
    return beginTransaction(database.get(), parent, flags);
}

inline Transaction
beginTransaction(const std::unique_ptr<Database> &database,
                 Transaction *parent,
                 TransactionFlags flags)
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

} // namespace db::storage

namespace db {

using storage::AccessMode;
using storage::Capability;
using storage::CursorHandle;
using storage::DatabaseOptions;
using storage::Options;
using storage::Store;
using storage::StoreFlags;
using storage::StoreOpenOptions;
using storage::Transaction;
using storage::TransactionFlags;

inline std::unique_ptr<Database>
createDatabaseFromEnvironment(DatabaseId variableName = "KOMAI_DB_BACKEND")
{
    return storage::createDatabaseFromEnvironment(variableName);
}

inline TransactionFlags
toAccessFlags(AccessMode mode) noexcept
{
    return storage::toAccessFlags(mode);
}

inline bool
supportsCapability(const Database &database, Capability capability) noexcept
{
    return storage::supportsCapability(database, capability);
}

inline void
requireCapabilities(const Database &database, StoreFlags flags)
{
    return storage::requireCapabilities(database, flags);
}

inline Transaction
beginTransaction(Database &database,
                 Transaction *parent = nullptr,
                 AccessMode mode     = AccessMode::ReadWrite)
{
    return storage::beginTransaction(database, parent, mode);
}

inline Transaction
beginTransaction(Database *database,
                 Transaction *parent = nullptr,
                 AccessMode mode     = AccessMode::ReadWrite)
{
    return storage::beginTransaction(database, parent, mode);
}

inline Transaction
beginTransaction(std::unique_ptr<Database> &database,
                 Transaction *parent = nullptr,
                 AccessMode mode     = AccessMode::ReadWrite)
{
    return storage::beginTransaction(database, parent, mode);
}

inline Transaction
beginTransaction(const std::unique_ptr<Database> &database,
                 Transaction *parent = nullptr,
                 AccessMode mode     = AccessMode::ReadWrite)
{
    return storage::beginTransaction(database, parent, mode);
}

inline Transaction
beginReadTransaction(Database &database, Transaction *parent = nullptr)
{
    return storage::beginReadTransaction(database, parent);
}

inline Transaction
beginReadTransaction(Database *database, Transaction *parent = nullptr)
{
    return storage::beginReadTransaction(database, parent);
}

inline Transaction
beginReadTransaction(std::unique_ptr<Database> &database, Transaction *parent = nullptr)
{
    return storage::beginReadTransaction(database, parent);
}

inline Transaction
beginReadTransaction(const std::unique_ptr<Database> &database, Transaction *parent = nullptr)
{
    return storage::beginReadTransaction(database, parent);
}

inline Transaction
beginWriteTransaction(Database &database, Transaction *parent = nullptr)
{
    return storage::beginWriteTransaction(database, parent);
}

inline Transaction
beginWriteTransaction(Database *database, Transaction *parent = nullptr)
{
    return storage::beginWriteTransaction(database, parent);
}

inline Transaction
beginWriteTransaction(std::unique_ptr<Database> &database, Transaction *parent = nullptr)
{
    return storage::beginWriteTransaction(database, parent);
}

inline Transaction
beginWriteTransaction(const std::unique_ptr<Database> &database, Transaction *parent = nullptr)
{
    return storage::beginWriteTransaction(database, parent);
}

inline void
open(Database &database, std::string_view directory, const DatabaseOptions &options = {})
{
    storage::open(database, directory, options);
}

inline void
open(Database *database, std::string_view directory, const DatabaseOptions &options = {})
{
    storage::open(database, directory, options);
}

inline void
open(std::unique_ptr<Database> &database,
     std::string_view directory,
     const DatabaseOptions &options = {})
{
    storage::open(database, directory, options);
}

inline void
open(const std::unique_ptr<Database> &database,
     std::string_view directory,
     const DatabaseOptions &options = {})
{
    storage::open(database, directory, options);
}

inline void
close(Database &database)
{
    storage::close(database);
}

inline void
close(Database *database)
{
    storage::close(database);
}

inline void
close(std::unique_ptr<Database> &database)
{
    storage::close(database);
}

inline void
close(const std::unique_ptr<Database> &database)
{
    storage::close(database);
}

inline storage::Cursor
openCursor(Transaction &txn, Store &store)
{
    return storage::openCursor(txn, store);
}

inline bool
ownsTransaction(const Database &database, const Transaction &transaction)
{
    return storage::ownsTransaction(database, transaction);
}

inline bool
ownsTransaction(const Database *database, const Transaction &transaction)
{
    return storage::ownsTransaction(database, transaction);
}

inline bool
ownsTransaction(const std::unique_ptr<Database> &database, const Transaction &transaction)
{
    return storage::ownsTransaction(database, transaction);
}

inline bool
ownsTransaction(std::unique_ptr<Database> &database, const Transaction &transaction)
{
    return storage::ownsTransaction(database, transaction);
}

inline std::vector<std::string>
listStoreNames(Database &database, Transaction &txn)
{
    return storage::listStoreNames(database, txn);
}

inline std::vector<std::string>
listStoreNames(Database *database, Transaction &txn)
{
    return storage::listStoreNames(database, txn);
}

inline std::vector<std::string>
listStoreNames(std::unique_ptr<Database> &database, Transaction &txn)
{
    return storage::listStoreNames(database, txn);
}

inline std::vector<std::string>
listStoreNames(const std::unique_ptr<Database> &database, Transaction &txn)
{
    return storage::listStoreNames(database, txn);
}

} // namespace db
