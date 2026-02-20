// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <exception>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <QString>

#include "db/Error.h"
#include "db/Flags.h"

namespace db {

class Txn;
class Dbi;

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

    virtual std::string_view id() const noexcept                                        = 0;
    virtual bool supportsCompaction() const noexcept                                    = 0;
    virtual void open(const QString &directory, const BackendOptions &options)          = 0;
    virtual void close() noexcept                                                       = 0;
    virtual bool isOpen() const noexcept                                                = 0;
    virtual Txn beginTxn(Txn *parent = nullptr, TxnFlags flags = TxnFlags::None)        = 0;
    virtual bool ownsTxn(const Txn &txn) const noexcept                                 = 0;
    virtual Dbi openDbi(Txn &txn, const char *name, const DbiOpenOptions &options = {}) = 0;
    virtual std::vector<std::string> listDbiNames(Txn &txn)                             = 0;
    virtual std::optional<std::size_t> mapSizeBytes() const noexcept                    = 0;
};

std::unique_ptr<Backend>
createDefaultBackend();
std::unique_ptr<Backend>
createBackend(std::string_view id);
std::unique_ptr<Backend>
createConfiguredBackend(std::string_view requestedId);

} // namespace db
