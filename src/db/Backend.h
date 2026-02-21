// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <array>
#include <cstddef>
#include <exception>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <span>
#include <vector>

#include "db/Error.h"
#include "db/DbTypes.h"
#include "db/CursorOp.h"
#include "db/Flags.h"

namespace db {

class Txn;
class Dbi;
class Cursor;
class Backend;

// Neutral-facing aliases. Prefer these names in new code while preserving
// existing names for compatibility.
using Database           = Backend;
using Store              = Dbi;
using Transaction        = Txn;
using CursorHandle       = Cursor;
using StoreHandle        = Store;
using DatabaseTransaction = Transaction;
using DatabaseStore      = Store;
using DatabaseId         = std::string_view;
using DatabaseIdSet      = std::span<const DatabaseId>;
using TransactionFlags   = TxnFlags;

// Generic names for common flags/options.
using AccessFlags = TxnFlags;
using WriteFlags  = PutFlags;
using MoveOp      = CursorOp;

// Canonical database identifiers.
inline constexpr std::string_view kMemoryDatabaseId{"memory"};
inline constexpr std::string_view kInMemoryDatabaseId{"in-memory"};
inline constexpr std::string_view kLmdbDatabaseId{"lmdb"};

// Backward-compatible backend identifiers.
inline constexpr std::string_view kMemoryBackendId = kMemoryDatabaseId;
inline constexpr std::string_view kInMemoryBackendId = kInMemoryDatabaseId;
inline constexpr std::string_view kLmdbBackendId = kLmdbDatabaseId;

enum class DupsortComparator
{
    StateKey,
    LegacyStateByKeyJson,
};

enum class Durability
{
    Durable,
    Relaxed,
};

enum class StorageCategory
{
    Persistent,
    Ephemeral,
};

enum class StoreCapability
{
    None,
    DuplicateKeys,
    IntegerKeys,
    PrefixScan,
};

struct BackendOptions
{
    std::size_t mapSizeBytes = 0;
    unsigned maxDbs          = 0;

    // Keep current cache behavior by default.
    Durability durability = Durability::Relaxed;
};

struct StoreOpenOptions
{
    StoreFlags flags                                  = StoreFlags::None;
    std::optional<DupsortComparator> dupsortComparator = std::nullopt;
};

// Existing backend option type retained for compatibility.
using DbiOpenOptions = StoreOpenOptions;

using StoreOptions       = StoreOpenOptions;
using DatabaseOptions    = BackendOptions;
using DatabaseCapability = StoreCapability;

class Backend
{
public:
    virtual ~Backend() = default;

    virtual std::string_view id() const noexcept                                             = 0;
    virtual StorageCategory storageCategory() const noexcept                                 = 0;
    virtual bool supportsCompaction() const noexcept                                         = 0;
    virtual void open(std::string_view directory, const BackendOptions &options)             = 0;
    virtual void close() noexcept                                                            = 0;
    virtual bool isOpen() const noexcept                                                     = 0;
    virtual Transaction beginTxn(Transaction *parent = nullptr, AccessFlags flags = AccessFlags::None) = 0;
    virtual bool ownsTxn(const Transaction &txn) const noexcept                                        = 0;
    virtual Store openStore(Transaction &txn,
                          std::string_view name,
                          const StoreOpenOptions &options = {})                                       = 0;
    virtual std::vector<std::string> listStoreNames(Transaction &txn)                                    = 0;
    virtual std::optional<std::size_t> mapSizeBytes() const noexcept                         = 0;
    virtual bool supports(StoreCapability capability) const noexcept
    {
        return capability == StoreCapability::None;
    }
};

std::unique_ptr<Backend>
createDefaultBackend();
std::unique_ptr<Backend>
createBackend(std::string_view id);
std::unique_ptr<Backend>
createConfiguredBackend(std::string_view requestedId);
std::unique_ptr<Backend>
createConfiguredBackendFromEnvironment(std::string_view variableName = "KOMAI_DB_BACKEND");
inline std::unique_ptr<Backend>
createDefaultDatabase()
{
    return createDefaultBackend();
}
inline std::unique_ptr<Backend>
createDatabase(DatabaseId id)
{
    return createBackend(id);
}
inline std::unique_ptr<Backend>
createConfiguredDatabase(DatabaseId requestedId = {})
{
    return createConfiguredBackend(requestedId);
}
inline std::unique_ptr<Backend>
createConfiguredDatabaseFromEnvironment(DatabaseId variableName = "KOMAI_DB_BACKEND")
{
    return createConfiguredBackendFromEnvironment(variableName);
}
bool
isBackendSupported(std::string_view id) noexcept;
bool
isDatabaseSupported(DatabaseId id) noexcept;
std::string_view
defaultBackendId() noexcept;
std::string_view
defaultDatabaseId() noexcept;
std::string_view
canonicalBackendId(std::string_view id) noexcept;
DatabaseId
canonicalDatabaseId(DatabaseId id) noexcept;
DatabaseIdSet
availableDatabaseIds() noexcept;
std::span<const std::string_view>
availableBackendIds() noexcept;

} // namespace db
