// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <memory>
#include <string_view>

#include "db/CursorOp.h"
#include "db/Flags.h"

namespace db::detail {

class TxnImpl
{
public:
    virtual ~TxnImpl() = default;

    virtual void commit()         = 0;
    virtual void abort()          = 0;
    virtual void renew()          = 0;
    virtual void reset() noexcept = 0;
};

class CursorImpl;

class DbiImpl
{
public:
    virtual ~DbiImpl() = default;

    virtual bool get(TxnImpl &txn, std::string_view key, std::string_view &value) = 0;
    virtual bool
    put(TxnImpl &txn, std::string_view key, std::string_view value, PutFlags flags) = 0;
    virtual bool del(TxnImpl &txn, std::string_view key)                            = 0;
    virtual bool del(TxnImpl &txn, std::string_view key, std::string_view value)    = 0;
    virtual bool drop(TxnImpl &txn, bool del)                                       = 0;
    virtual std::size_t size(TxnImpl &txn)                                          = 0;

    virtual std::unique_ptr<CursorImpl> openCursor(TxnImpl &txn) = 0;
};

class CursorImpl
{
public:
    virtual ~CursorImpl() = default;

    virtual bool get(std::string_view &key, std::string_view &value, CursorOp op)  = 0;
    virtual bool get(std::string_view &key, CursorOp op)                           = 0;
    virtual bool put(std::string_view key, std::string_view value, PutFlags flags) = 0;
    virtual bool del(unsigned flags)                                               = 0;
    virtual void close()                                                           = 0;
};

} // namespace db::detail
