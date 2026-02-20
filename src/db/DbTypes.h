// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>

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

class TxnImpl
{
public:
    virtual ~TxnImpl() = default;

    virtual void commit()         = 0;
    virtual void abort()          = 0;
    virtual void renew()          = 0;
    virtual void reset() noexcept = 0;
};
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
    explicit Txn(std::shared_ptr<detail::TxnImpl> impl)
      : impl_(std::move(impl))
    {
    }

    Txn(const Txn &)                = delete;
    Txn &operator=(const Txn &)     = delete;
    Txn(Txn &&) noexcept            = default;
    Txn &operator=(Txn &&) noexcept = default;

    void commit()
    {
        if (!impl_)
            throw Error("Invalid database transaction", ErrorKind::Invalid);
        impl_->commit();
    }
    void abort()
    {
        if (!impl_)
            throw Error("Invalid database transaction", ErrorKind::Invalid);
        impl_->abort();
    }
    void renew()
    {
        if (!impl_)
            throw Error("Invalid database transaction", ErrorKind::Invalid);
        impl_->renew();
    }
    void reset() noexcept
    {
        if (impl_)
            impl_->reset();
    }

private:
    friend class Dbi;
    friend class Cursor;
    friend detail::TxnImpl *detail::txnImpl(Txn &txn) noexcept;
    friend const detail::TxnImpl *detail::txnImpl(const Txn &txn) noexcept;
    detail::TxnImpl &implRef() { return *impl_; }
    const detail::TxnImpl &implRef() const { return *impl_; }
    std::shared_ptr<detail::TxnImpl> impl_;
};

class Dbi
{
public:
    Dbi() = default;
    explicit Dbi(std::shared_ptr<detail::DbiImpl> impl)
      : impl_(std::move(impl))
    {
    }

    Dbi(const Dbi &)                = default;
    Dbi &operator=(const Dbi &)     = default;
    Dbi(Dbi &&) noexcept            = default;
    Dbi &operator=(Dbi &&) noexcept = default;

    template<typename Key, typename Value>
    bool get(Txn &txn, const Key &key, Value &value)
    {
        if (!impl_)
            throw Error("Invalid database handle", ErrorKind::Invalid);
        std::string_view result;
        const bool found = impl_->get(txn.implRef(), detail::toBytes(key), result);
        if (found)
            detail::assignBytes(value, result);
        return found;
    }

    template<typename Key, typename Value>
    bool put(Txn &txn, const Key &key, const Value &value, PutFlags flags = PutFlags::None)
    {
        if (!impl_)
            throw Error("Invalid database handle", ErrorKind::Invalid);
        return impl_->put(txn.implRef(), detail::toBytes(key), detail::toBytes(value), flags);
    }

    template<typename Key>
    bool del(Txn &txn, const Key &key)
    {
        if (!impl_)
            throw Error("Invalid database handle", ErrorKind::Invalid);
        return impl_->del(txn.implRef(), detail::toBytes(key));
    }

    template<typename Key, typename Value>
    bool del(Txn &txn, const Key &key, const Value &value)
    {
        if (!impl_)
            throw Error("Invalid database handle", ErrorKind::Invalid);
        return impl_->del(txn.implRef(), detail::toBytes(key), detail::toBytes(value));
    }

    bool drop(Txn &txn, bool del = false)
    {
        if (!impl_)
            throw Error("Invalid database handle", ErrorKind::Invalid);
        return impl_->drop(txn.implRef(), del);
    }

    std::size_t size(Txn &txn)
    {
        if (!impl_)
            throw Error("Invalid database handle", ErrorKind::Invalid);
        return impl_->size(txn.implRef());
    }

private:
    friend class Cursor;
    friend detail::DbiImpl *detail::dbiImpl(Dbi &dbi) noexcept;
    friend const detail::DbiImpl *detail::dbiImpl(const Dbi &dbi) noexcept;
    detail::DbiImpl &implRef() { return *impl_; }
    const detail::DbiImpl &implRef() const { return *impl_; }
    std::shared_ptr<detail::DbiImpl> impl_;
};

class Cursor
{
public:
    Cursor() = default;
    explicit Cursor(std::unique_ptr<detail::CursorImpl> impl)
      : impl_(std::move(impl))
    {
    }

    Cursor(const Cursor &)                = delete;
    Cursor &operator=(const Cursor &)     = delete;
    Cursor(Cursor &&) noexcept            = default;
    Cursor &operator=(Cursor &&) noexcept = default;

    static Cursor open(Txn &txn, Dbi dbi)
    {
        if (!dbi.impl_)
            throw Error("Invalid database handle", ErrorKind::Invalid);
        return Cursor{dbi.impl_->openCursor(txn.implRef())};
    }

    template<typename Key, typename Value>
    bool get(Key &key, Value &value, CursorOp op)
    {
        if (!impl_)
            throw Error("Invalid database cursor", ErrorKind::Invalid);

        std::string_view keyBytes = detail::toBytes(key);
        std::string_view valueBytes;
        const bool found = impl_->get(keyBytes, valueBytes, op);
        if (found) {
            detail::assignBytes(key, keyBytes);
            detail::assignBytes(value, valueBytes);
        }
        return found;
    }

    template<typename Key>
    bool get(Key &key, CursorOp op)
    {
        if (!impl_)
            throw Error("Invalid database cursor", ErrorKind::Invalid);

        std::string_view keyBytes = detail::toBytes(key);
        const bool found          = impl_->get(keyBytes, op);
        if (found)
            detail::assignBytes(key, keyBytes);
        return found;
    }

    template<typename Key, typename Value>
    bool put(const Key &key, const Value &value, PutFlags flags = PutFlags::None)
    {
        if (!impl_)
            throw Error("Invalid database cursor", ErrorKind::Invalid);

        return impl_->put(detail::toBytes(key), detail::toBytes(value), flags);
    }

    bool del(unsigned flags = 0)
    {
        if (!impl_)
            throw Error("Invalid database cursor", ErrorKind::Invalid);

        return impl_->del(flags);
    }
    void close()
    {
        if (!impl_)
            throw Error("Invalid database cursor", ErrorKind::Invalid);

        impl_->close();
    }

private:
    std::unique_ptr<detail::CursorImpl> impl_;
};

namespace detail {

inline TxnImpl *
txnImpl(Txn &txn) noexcept
{
    return txn.impl_.get();
}

inline const TxnImpl *
txnImpl(const Txn &txn) noexcept
{
    return txn.impl_.get();
}

inline DbiImpl *
dbiImpl(Dbi &dbi) noexcept
{
    return dbi.impl_.get();
}

inline const DbiImpl *
dbiImpl(const Dbi &dbi) noexcept
{
    return dbi.impl_.get();
}

} // namespace detail

} // namespace db
