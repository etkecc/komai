// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>
#include <utility>

#include "Backend.h"

namespace db {

class LmdbBackend final : public Backend
{
public:
    LmdbBackend();
    ~LmdbBackend() override;

    std::string_view id() const noexcept override { return kLmdbBackendId; }
    StorageCategory storageCategory() const noexcept override { return StorageCategory::Persistent; }
    bool supportsCompaction() const noexcept override { return true; }

    void open(std::string_view directory, const BackendOptions &options) override;
    void close() noexcept override;
    bool isOpen() const noexcept override;
    Txn beginTxn(Txn *parent = nullptr, TxnFlags flags = TxnFlags::None) override;
    bool ownsTxn(const Txn &txn) const noexcept override;
    Dbi openStore(Txn &txn, std::string_view name, const DbiOpenOptions &options = {}) override;
    std::vector<std::string> listStoreNames(Txn &txn) override;
    std::optional<std::size_t> mapSizeBytes() const noexcept override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace db
