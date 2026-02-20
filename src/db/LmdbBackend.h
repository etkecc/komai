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

    std::string_view id() const noexcept override { return "lmdb"; }

    void open(const QString &directory, const BackendOptions &options) override;
    void close() noexcept override;
    bool isOpen() const noexcept override;
    Txn beginTxn(Txn *parent = nullptr, TxnFlags flags = TxnFlags::None) override;
    bool ownsTxn(const Txn &txn) const noexcept override;
    Dbi openDbi(Txn &txn, const char *name = nullptr, DbiFlags flags = DbiFlags::None) override;
    void setDbiDupsort(Txn &txn, Dbi dbi, DupsortComparator comparator) override;
    void closeDbi(Dbi dbi) noexcept override;
    std::optional<std::size_t> mapSizeBytes() const noexcept override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace db
