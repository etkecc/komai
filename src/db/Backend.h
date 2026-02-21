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
#include "db/Flags.h"

namespace db {

class Txn;
class Dbi;

inline constexpr std::string_view kMemoryBackendId{"memory"};
inline constexpr std::string_view kInMemoryBackendId{"in-memory"};
inline constexpr std::string_view kLmdbBackendId{"lmdb"};

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

struct BackendOptions
{
    std::size_t mapSizeBytes = 0;
    unsigned maxDbs          = 0;

    // Keep current cache behavior by default.
    Durability durability = Durability::Relaxed;
};

struct DbiOpenOptions
{
    DbiFlags flags                                     = DbiFlags::None;
    std::optional<DupsortComparator> dupsortComparator = std::nullopt;
};

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
    virtual Txn beginTxn(Txn *parent = nullptr, TxnFlags flags = TxnFlags::None)             = 0;
    virtual bool ownsTxn(const Txn &txn) const noexcept                                      = 0;
    virtual Dbi openDbi(Txn &txn, std::string_view name, const DbiOpenOptions &options = {}) = 0;
    virtual std::vector<std::string> listDbiNames(Txn &txn)                                  = 0;
    virtual std::optional<std::size_t> mapSizeBytes() const noexcept                         = 0;
};

std::unique_ptr<Backend>
createDefaultBackend();
std::unique_ptr<Backend>
createBackend(std::string_view id);
std::unique_ptr<Backend>
createConfiguredBackend(std::string_view requestedId);
std::unique_ptr<Backend>
createConfiguredBackendFromEnvironment(std::string_view variableName = "KOMAI_DB_BACKEND");
bool
isBackendSupported(std::string_view id) noexcept;
std::string_view
defaultBackendId() noexcept;
std::string_view
canonicalBackendId(std::string_view id) noexcept;
std::span<const std::string_view>
availableBackendIds() noexcept;

} // namespace db
