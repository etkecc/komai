// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <utility>

#include "Backend.h"
#include "db/LmdbHeaders.h"

namespace db {

class LmdbBackend final : public Backend
{
public:
    std::string_view id() const noexcept override { return "lmdb"; }

    void open(const QString &directory, const BackendOptions &options) override;
    void close() noexcept override;
    bool isOpen() const noexcept override { return env_.handle() != nullptr; }
    bool isMapFullError(const std::exception &e) const noexcept override;
    Txn beginTxn(Txn *parent = nullptr, unsigned flags = 0) override;
    const void *nativeHandle() const noexcept override { return env_.handle(); }
    void closeDbi(Dbi dbi) noexcept override;
    std::optional<std::size_t> mapSizeBytes() const noexcept override;

    lmdb::env &env() noexcept { return env_; }
    const lmdb::env &env() const noexcept { return env_; }

private:
    lmdb::env env_ = nullptr;
};

} // namespace db
