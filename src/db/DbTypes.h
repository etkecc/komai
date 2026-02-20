// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>

#include "db/CursorOp.h"
#include "db/Error.h"
#include "db/Flags.h"
#include "db/Serde.h"

namespace db {

class Txn;
class Dbi;

namespace detail {

class TxnImpl;
class DbiImpl;
class CursorImpl;

TxnImpl *
txnImpl(Txn &txn) noexcept;
const TxnImpl *
txnImpl(const Txn &txn) noexcept;
DbiImpl *
dbiImpl(Dbi &dbi) noexcept;
const DbiImpl *
dbiImpl(const Dbi &dbi) noexcept;

template<typename T>
inline constexpr bool alwaysFalseV = false;

template<typename T>
std::string_view
toBytes(const T &value)
{
    return toSv(value);
}

template<typename T>
void
assignBytes(T &out, std::string_view value)
{
    using U = std::remove_cvref_t<T>;

    if constexpr (std::is_same_v<U, std::string>) {
        out.assign(value.data(), value.size());
    } else if constexpr (std::is_same_v<U, std::string_view>) {
        out = value;
    } else if constexpr (std::is_integral_v<U> || std::is_enum_v<U>) {
        out = fromSv<U>(value);
    } else {
        static_assert(alwaysFalseV<U>, "Unsupported key/value type for db operation");
    }
}

} // namespace detail

class Txn
{
public:
    Txn() = default;
    explicit Txn(std::shared_ptr<detail::TxnImpl> impl);

    Txn(const Txn &)                = delete;
    Txn &operator=(const Txn &)     = delete;
    Txn(Txn &&) noexcept            = default;
    Txn &operator=(Txn &&) noexcept = default;

    void commit();
    void abort();
    void renew();
    void reset() noexcept;

private:
    friend class Dbi;
    friend class Cursor;
    friend detail::TxnImpl *detail::txnImpl(Txn &txn) noexcept;
    friend const detail::TxnImpl *detail::txnImpl(const Txn &txn) noexcept;

    detail::TxnImpl &implRef();
    const detail::TxnImpl &implRef() const;

    std::shared_ptr<detail::TxnImpl> impl_;
};

class Dbi
{
public:
    Dbi() = default;
    explicit Dbi(std::shared_ptr<detail::DbiImpl> impl);

    Dbi(const Dbi &)                = default;
    Dbi &operator=(const Dbi &)     = default;
    Dbi(Dbi &&) noexcept            = default;
    Dbi &operator=(Dbi &&) noexcept = default;

    template<typename Key, typename Value>
    bool get(Txn &txn, const Key &key, Value &value)
    {
        std::string_view result;
        const bool found = getRaw(txn, detail::toBytes(key), result);
        if (found)
            detail::assignBytes(value, result);
        return found;
    }

    template<typename Key, typename Value>
    bool put(Txn &txn, const Key &key, const Value &value, PutFlags flags = PutFlags::None)
    {
        return putRaw(txn, detail::toBytes(key), detail::toBytes(value), flags);
    }

    template<typename Key>
    bool del(Txn &txn, const Key &key)
    {
        return delRaw(txn, detail::toBytes(key));
    }

    template<typename Key, typename Value>
    bool del(Txn &txn, const Key &key, const Value &value)
    {
        return delRaw(txn, detail::toBytes(key), detail::toBytes(value));
    }

    bool drop(Txn &txn, bool del = false);
    std::size_t size(Txn &txn);

private:
    friend class Cursor;
    friend detail::DbiImpl *detail::dbiImpl(Dbi &dbi) noexcept;
    friend const detail::DbiImpl *detail::dbiImpl(const Dbi &dbi) noexcept;

    bool getRaw(Txn &txn, std::string_view key, std::string_view &value);
    bool putRaw(Txn &txn, std::string_view key, std::string_view value, PutFlags flags);
    bool delRaw(Txn &txn, std::string_view key);
    bool delRaw(Txn &txn, std::string_view key, std::string_view value);

    detail::DbiImpl &implRef();
    const detail::DbiImpl &implRef() const;

    std::shared_ptr<detail::DbiImpl> impl_;
};

class Cursor
{
public:
    Cursor() = default;
    explicit Cursor(std::unique_ptr<detail::CursorImpl> impl);
    ~Cursor();

    Cursor(const Cursor &)            = delete;
    Cursor &operator=(const Cursor &) = delete;
    Cursor(Cursor &&) noexcept;
    Cursor &operator=(Cursor &&) noexcept;

    static Cursor open(Txn &txn, Dbi dbi);

    template<typename Key, typename Value>
    bool get(Key &key, Value &value, CursorOp op)
    {
        std::string_view keyBytes = detail::toBytes(key);
        std::string_view valueBytes;
        const bool found = getRaw(keyBytes, valueBytes, op);
        if (found) {
            detail::assignBytes(key, keyBytes);
            detail::assignBytes(value, valueBytes);
        }
        return found;
    }

    template<typename Key>
    bool get(Key &key, CursorOp op)
    {
        std::string_view keyBytes = detail::toBytes(key);
        const bool found          = getRaw(keyBytes, op);
        if (found)
            detail::assignBytes(key, keyBytes);
        return found;
    }

    template<typename Key, typename Value>
    bool put(const Key &key, const Value &value, PutFlags flags = PutFlags::None)
    {
        return putRaw(detail::toBytes(key), detail::toBytes(value), flags);
    }

    bool del(unsigned flags = 0);
    void close();

private:
    bool getRaw(std::string_view &key, std::string_view &value, CursorOp op);
    bool getRaw(std::string_view &key, CursorOp op);
    bool putRaw(std::string_view key, std::string_view value, PutFlags flags);
    bool delRaw(unsigned flags);
    void closeRaw();

    std::unique_ptr<detail::CursorImpl> impl_;
};

} // namespace db
