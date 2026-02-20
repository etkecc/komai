// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <exception>
#include <memory>
#include <optional>
#include <string_view>

#include <QString>

#include "db/DbTypes.h"

namespace db {

struct BackendOptions
{
    std::size_t mapSizeBytes = 0;
    unsigned maxDbs          = 0;

    // Keep current cache behavior by default.
    bool noMetaSync = true;
    bool noSync     = true;
};

class Backend
{
public:
    virtual ~Backend() = default;

    virtual std::string_view id() const noexcept                               = 0;
    virtual void open(const QString &directory, const BackendOptions &options) = 0;
    virtual void close() noexcept                                              = 0;
    virtual bool isOpen() const noexcept                                       = 0;
    virtual bool isMapFullError(const std::exception &e) const noexcept        = 0;
    virtual Txn beginTxn(Txn *parent = nullptr, unsigned flags = 0)            = 0;
    virtual const void *nativeHandle() const noexcept                          = 0;
    virtual void closeDbi(Dbi dbi) noexcept                                    = 0;
    virtual std::optional<std::size_t> mapSizeBytes() const noexcept           = 0;
};

std::unique_ptr<Backend>
createDefaultBackend();

} // namespace db
