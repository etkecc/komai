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
#include "db/Catalog.h"
#include "db/DupIndex.h"
#include "db/Error.h"
#include "db/Json.h"
#include "db/MegolmIndex.h"
#include "db/MemberInfo.h"
#include "db/NamePolicy.h"
#include "db/OlmSessionIndex.h"
#include "db/OrderEntry.h"
#include "db/ReadReceiptIndex.h"
#include "db/RoomInfo.h"
#include "db/Scan.h"
#include "db/Serde.h"
#include "db/StateIndex.h"
#include "db/SyncState.h"
#include "db/TimelineIndex.h"

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
using ScanDirection    = db::ScanDirection;
using StoreCapability  = db::StoreCapability;

using db::findStateEventId;
using db::forEachOlmSessionForCurve;
using db::getInboundMegolmSessionValue;
using db::getJsonValue;
using db::getMegolmSessionDataValue;
using db::getMemberInfo;
using db::getReadReceiptValue;
using db::getRoomInfo;
using db::getSyncStateJsonValue;
using db::getSyncStateSecretValue;
using db::getSyncStateValue;
using db::listOlmSessionIds;
using db::listStateEventIds;
using db::megolmSessionKey;
using db::parseMegolmSessionKey;
using db::parseMemberInfo;
using db::parseRoomInfo;
using db::putInboundMegolmSessionValue;
using db::putJsonValue;
using db::putMegolmSessionDataValue;
using db::putMemberInfo;
using db::putOlmSessionValue;
using db::putReadReceiptValue;
using db::putRoomInfo;
using db::putStateEventId;
using db::putSyncStateJsonValue;
using db::putSyncStateSecretValue;
using db::putSyncStateValue;
using db::removeStateEventId;
using db::removeSyncStateSecretValue;
using db::removeSyncStateValue;
using db::serializeOrderEntry;
using db::serializeRoomInfo;
using db::toSv;

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

inline StoreOpenOptions
openOptionsForName(std::string_view name)
{
    return ::db::openOptionsForName(name);
}

inline StoreOpenOptions
openOptionsForGlobal(catalog::GlobalDb db)
{
    return ::db::openOptionsForGlobal(db);
}

inline StoreOpenOptions
openOptionsForRoom(catalog::RoomDb db)
{
    return ::db::openOptionsForRoom(db);
}

using db::appendEventOrderEntry;
using db::appendMessageOrderEntry;
using db::cleanupTimelineBeforePrevBatchMarker;
using db::eraseEntriesIf;
using db::eventIndexForEvent;
using db::firstEntry;
using db::firstOrderedIndex;
using db::firstPrevBatchToken;
using db::forEachDupValue;
using db::forEachEntry;
using db::forEachEntryFromKey;
using db::forEachEntryWithPrefix;
using db::forEachUniqueKey;
using db::lastEntry;
using db::lastInvisibleEventAfter;
using db::lastOrderedIndex;
using db::lastTimelineEventId;
using db::lastVisibleEvent;
using db::listDupValues;
using db::listEntries;
using db::listKeys;
using db::listOrderEntriesAfterPrevBatchMarker;
using db::listOrderEntryEventIds;
using db::listUniqueKeys;
using db::prependEventOrderEntry;
using db::prependMessageOrderEntry;
using db::putDupValueForKeys;
using db::putEventOrderMapping;
using db::putEventOrderMappingForEvent;
using db::putMessageOrderMapping;
using db::putOrderEntry;
using db::removeMessageOrderMapping;
using db::removeMessageOrderMappingsNotInOrderEntries;
using db::removeOrderEntryReferences;
using db::removeOrderEntryWithReferences;
using db::removePendingEntriesByTxnId;
using db::removeTimelineEventReferences;
using db::replaceDupValueForKeys;
using db::replaceTimelineEventId;
using db::setOrderEntryPrevBatch;
using db::timelineEventIdAtIndex;
using db::timelineIndexForEvent;
using db::timelineRange;
using db::trimOldestOrderEntriesWithReferences;

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

inline Store
openNamedStore(Database &database,
               Transaction &txn,
               std::string_view name,
               bool create      = true,
               StoreFlags flags = StoreFlags::None)
{
    auto options = openOptionsForName(name);
    options.flags |= flags;
    if (create)
        options.flags |= StoreFlags::Create;

    requireCapabilities(database, options.flags);
    return database.openStore(txn, name, options);
}

inline Store
openNamedStore(Database *database,
               Transaction &txn,
               std::string_view name,
               bool create      = true,
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
               bool create      = true,
               StoreFlags flags = StoreFlags::None)
{
    return openNamedStore(database.get(), txn, name, create, flags);
}

inline Store
openNamedStore(const std::unique_ptr<Database> &database,
               Transaction &txn,
               std::string_view name,
               bool create      = true,
               StoreFlags flags = StoreFlags::None)
{
    return openNamedStore(database.get(), txn, name, create, flags);
}

inline Store
openStore(Database &database,
          Transaction &txn,
          std::string_view name,
          bool create      = true,
          StoreFlags flags = StoreFlags::None)
{
    return openNamedStore(database, txn, name, create, flags);
}

inline Store
openStore(Database *database,
          Transaction &txn,
          std::string_view name,
          bool create      = true,
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
          bool create      = true,
          StoreFlags flags = StoreFlags::None)
{
    return openNamedStore(database, txn, name, create, flags);
}

inline Store
openStore(const std::unique_ptr<Database> &database,
          Transaction &txn,
          std::string_view name,
          bool create      = true,
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
openStore(std::unique_ptr<Database> &database,
          Transaction &txn,
          std::string_view name,
          const Options &options)
{
    return openStore(database.get(), txn, name, options);
}

inline Store
openStore(const std::unique_ptr<Database> &database,
          Transaction &txn,
          std::string_view name,
          const Options &options)
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
openGlobalStore(std::unique_ptr<Database> &database,
                Transaction &txn,
                catalog::GlobalDb store,
                bool create = true)
{
    return openGlobalStore(database.get(), txn, store, create);
}

inline Store
openGlobalStore(const std::unique_ptr<Database> &database,
                Transaction &txn,
                catalog::GlobalDb store,
                bool create = true)
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

namespace db {

using storage::AccessMode;
using storage::Capability;
using storage::CursorHandle;
using storage::DatabaseOptions;
using storage::Options;
using storage::ScanDirection;
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

inline Store
openNamedStore(Database &database,
               Transaction &txn,
               std::string_view name,
               bool create      = true,
               StoreFlags flags = StoreFlags::None)
{
    return storage::openNamedStore(database, txn, name, create, flags);
}

inline Store
openNamedStore(Database *database,
               Transaction &txn,
               std::string_view name,
               bool create      = true,
               StoreFlags flags = StoreFlags::None)
{
    return storage::openNamedStore(database, txn, name, create, flags);
}

inline Store
openNamedStore(std::unique_ptr<Database> &database,
               Transaction &txn,
               std::string_view name,
               bool create      = true,
               StoreFlags flags = StoreFlags::None)
{
    return storage::openNamedStore(database, txn, name, create, flags);
}

inline Store
openNamedStore(const std::unique_ptr<Database> &database,
               Transaction &txn,
               std::string_view name,
               bool create      = true,
               StoreFlags flags = StoreFlags::None)
{
    return storage::openNamedStore(database, txn, name, create, flags);
}

inline Store
openStore(Database &database,
          Transaction &txn,
          std::string_view name,
          bool create      = true,
          StoreFlags flags = StoreFlags::None)
{
    return storage::openStore(database, txn, name, create, flags);
}

inline Store
openStore(Database *database,
          Transaction &txn,
          std::string_view name,
          bool create      = true,
          StoreFlags flags = StoreFlags::None)
{
    return storage::openStore(database, txn, name, create, flags);
}

inline Store
openStore(std::unique_ptr<Database> &database,
          Transaction &txn,
          std::string_view name,
          bool create      = true,
          StoreFlags flags = StoreFlags::None)
{
    return storage::openStore(database, txn, name, create, flags);
}

inline Store
openStore(const std::unique_ptr<Database> &database,
          Transaction &txn,
          std::string_view name,
          bool create      = true,
          StoreFlags flags = StoreFlags::None)
{
    return storage::openStore(database, txn, name, create, flags);
}

inline Store
openStore(Database &database,
          Transaction &txn,
          std::string_view name,
          const StoreOpenOptions &options)
{
    return storage::openStore(database, txn, name, options);
}

inline Store
openStore(Database *database,
          Transaction &txn,
          std::string_view name,
          const StoreOpenOptions &options)
{
    return storage::openStore(database, txn, name, options);
}

inline Store
openStore(std::unique_ptr<Database> &database,
          Transaction &txn,
          std::string_view name,
          const StoreOpenOptions &options)
{
    return storage::openStore(database, txn, name, options);
}

inline Store
openStore(const std::unique_ptr<Database> &database,
          Transaction &txn,
          std::string_view name,
          const StoreOpenOptions &options)
{
    return storage::openStore(database, txn, name, options);
}

inline Store
openGlobalStore(Database &database, Transaction &txn, catalog::GlobalDb store, bool create = true)
{
    return storage::openGlobalStore(database, txn, store, create);
}

inline Store
openGlobalStore(Database *database, Transaction &txn, catalog::GlobalDb store, bool create = true)
{
    return storage::openGlobalStore(database, txn, store, create);
}

inline Store
openGlobalStore(std::unique_ptr<Database> &database,
                Transaction &txn,
                catalog::GlobalDb store,
                bool create = true)
{
    return storage::openGlobalStore(database, txn, store, create);
}

inline Store
openGlobalStore(const std::unique_ptr<Database> &database,
                Transaction &txn,
                catalog::GlobalDb store,
                bool create = true)
{
    return storage::openGlobalStore(database, txn, store, create);
}

inline Store
openRoomStore(Database &database,
              Transaction &txn,
              std::string_view roomId,
              catalog::RoomDb store,
              bool create = true)
{
    return storage::openRoomStore(database, txn, roomId, store, create);
}

inline Store
openRoomStore(Database *database,
              Transaction &txn,
              std::string_view roomId,
              catalog::RoomDb store,
              bool create = true)
{
    return storage::openRoomStore(database, txn, roomId, store, create);
}

inline Store
openRoomStore(std::unique_ptr<Database> &database,
              Transaction &txn,
              std::string_view roomId,
              catalog::RoomDb store,
              bool create = true)
{
    return storage::openRoomStore(database, txn, roomId, store, create);
}

inline Store
openRoomStore(const std::unique_ptr<Database> &database,
              Transaction &txn,
              std::string_view roomId,
              catalog::RoomDb store,
              bool create = true)
{
    return storage::openRoomStore(database, txn, roomId, store, create);
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
