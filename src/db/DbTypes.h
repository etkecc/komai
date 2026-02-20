// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <utility>

#include "db/CursorOp.h"
#include "db/LmdbError.h"
#include "db/LmdbFlags.h"

namespace db {

class Txn
{
public:
    Txn() = default;
    explicit Txn(lmdb::txn native)
      : native_(std::move(native))
    {
    }

    Txn(const Txn &)                = delete;
    Txn &operator=(const Txn &)     = delete;
    Txn(Txn &&) noexcept            = default;
    Txn &operator=(Txn &&) noexcept = default;

    static Txn fromNative(lmdb::txn native) { return Txn{std::move(native)}; }

    void commit()
    {
        translateLmdbErrors([&] { native_.commit(); });
    }
    void abort()
    {
        translateLmdbErrors([&] { native_.abort(); });
    }
    void renew()
    {
        translateLmdbErrors([&] { native_.renew(); });
    }
    void reset() noexcept { native_.reset(); }

private:
    friend class Dbi;
    friend class Cursor;
    friend class LmdbBackend;

    lmdb::txn &native() noexcept { return native_; }
    const lmdb::txn &native() const noexcept { return native_; }
    auto handle() const noexcept { return native_.handle(); }
    const void *env() const noexcept { return native_.env(); }

    lmdb::txn native_;
};

class Dbi
{
public:
    Dbi() = default;
    explicit Dbi(lmdb::dbi native)
      : native_(std::move(native))
    {
    }

    Dbi(const Dbi &)                = default;
    Dbi &operator=(const Dbi &)     = default;
    Dbi(Dbi &&) noexcept            = default;
    Dbi &operator=(Dbi &&) noexcept = default;

    static Dbi fromNative(lmdb::dbi native) { return Dbi{std::move(native)}; }

    template<typename Key, typename Value>
    decltype(auto) get(Txn &txn, Key &&key, Value &value)
    {
        return translateLmdbErrors(
          [&] { return native_.get(txn.native(), std::forward<Key>(key), value); });
    }

    template<typename Key, typename Value>
    decltype(auto) put(Txn &txn, Key &&key, Value &&value, PutFlags flags = PutFlags::None)
    {
        return translateLmdbErrors([&] {
            return native_.put(txn.native(),
                               std::forward<Key>(key),
                               std::forward<Value>(value),
                               toLmdbPutFlags(flags));
        });
    }

    template<typename Key>
    decltype(auto) del(Txn &txn, Key &&key)
    {
        return translateLmdbErrors(
          [&] { return native_.del(txn.native(), std::forward<Key>(key)); });
    }

    template<typename Key, typename Value>
    decltype(auto) del(Txn &txn, Key &&key, Value &&value)
    {
        return translateLmdbErrors([&] {
            return native_.del(txn.native(), std::forward<Key>(key), std::forward<Value>(value));
        });
    }

    decltype(auto) drop(Txn &txn, bool del = false)
    {
        return translateLmdbErrors([&] { return native_.drop(txn.native(), del); });
    }

    decltype(auto) size(Txn &txn)
    {
        return translateLmdbErrors([&] { return native_.size(txn.native()); });
    }

private:
    friend class Cursor;
    friend class LmdbBackend;

    lmdb::dbi &native() noexcept { return native_; }
    const lmdb::dbi &native() const noexcept { return native_; }

    lmdb::dbi native_;
};

class Cursor
{
public:
    Cursor() = default;
    explicit Cursor(lmdb::cursor native)
      : native_(std::move(native))
    {
    }

    Cursor(const Cursor &)                = delete;
    Cursor &operator=(const Cursor &)     = delete;
    Cursor(Cursor &&) noexcept            = default;
    Cursor &operator=(Cursor &&) noexcept = default;

    static Cursor fromNative(lmdb::cursor native) { return Cursor{std::move(native)}; }
    static Cursor open(Txn &txn, Dbi dbi)
    {
        return translateLmdbErrors(
          [&] { return fromNative(lmdb::cursor::open(txn.native(), dbi.native())); });
    }

    template<typename Key, typename Value>
    decltype(auto) get(Key &&key, Value &&value, CursorOp op)
    {
        return translateLmdbErrors([&] {
            return native_.get(std::forward<Key>(key), std::forward<Value>(value), toNative(op));
        });
    }

    template<typename Key>
    decltype(auto) get(Key &&key, CursorOp op)
    {
        return translateLmdbErrors(
          [&] { return native_.get(std::forward<Key>(key), toNative(op)); });
    }

    template<typename Key, typename Value>
    decltype(auto) put(Key &&key, Value &&value, PutFlags flags = PutFlags::None)
    {
        return translateLmdbErrors([&] {
            return native_.put(
              std::forward<Key>(key), std::forward<Value>(value), toLmdbPutFlags(flags));
        });
    }

    decltype(auto) del(unsigned flags = 0)
    {
        return translateLmdbErrors([&] { return native_.del(flags); });
    }
    void close()
    {
        translateLmdbErrors([&] { native_.close(); });
    }

private:
    static constexpr MDB_cursor_op toNative(CursorOp op)
    {
        switch (op) {
        case CursorOp::First:
            return MDB_FIRST;
        case CursorOp::FirstDup:
            return MDB_FIRST_DUP;
        case CursorOp::GetBoth:
            return MDB_GET_BOTH;
        case CursorOp::Last:
            return MDB_LAST;
        case CursorOp::Next:
            return MDB_NEXT;
        case CursorOp::NextDup:
            return MDB_NEXT_DUP;
        case CursorOp::NextNoDup:
            return MDB_NEXT_NODUP;
        case CursorOp::Prev:
            return MDB_PREV;
        case CursorOp::Set:
            return MDB_SET;
        case CursorOp::SetRange:
            return MDB_SET_RANGE;
        }
        return MDB_NEXT;
    }

    lmdb::cursor native_;
};

} // namespace db
