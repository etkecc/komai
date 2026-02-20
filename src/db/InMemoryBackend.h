// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>
#include <optional>

#include "db/Backend.h"

namespace db {

class InMemoryBackend final : public Backend
{
public:
    struct Impl;

    InMemoryBackend();
    ~InMemoryBackend() override;

    std::string_view id() const noexcept override { return "memory"; }

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
    std::unique_ptr<Impl> impl_;
};

} // namespace db
