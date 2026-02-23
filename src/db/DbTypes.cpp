// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "db/DbTypes.h"

#include <utility>

#include "db/Internal.h"

namespace db {

Txn::Txn(std::shared_ptr<detail::TxnImpl> impl)
  : impl_(std::move(impl))
{
}

void
Txn::commit()
{
    if (!impl_)
        throw Error("Invalid database transaction", ErrorKind::Invalid);
    impl_->commit();
}

void
Txn::abort()
{
    if (!impl_)
        throw Error("Invalid database transaction", ErrorKind::Invalid);
    impl_->abort();
}

void
Txn::renew()
{
    if (!impl_)
        throw Error("Invalid database transaction", ErrorKind::Invalid);
    impl_->renew();
}

void
Txn::reset() noexcept
{
    if (impl_)
        impl_->reset();
}

detail::TxnImpl &
Txn::implRef()
{
    return *impl_;
}

const detail::TxnImpl &
Txn::implRef() const
{
    return *impl_;
}

Dbi::Dbi(std::shared_ptr<detail::DbiImpl> impl)
  : impl_(std::move(impl))
{
}

bool
Dbi::getRaw(Txn &txn, std::string_view key, std::string_view &value)
{
    if (!impl_)
        throw Error("Invalid database handle", ErrorKind::Invalid);

    return impl_->get(txn.implRef(), key, value);
}

bool
Dbi::putRaw(Txn &txn, std::string_view key, std::string_view value, PutFlags flags)
{
    if (!impl_)
        throw Error("Invalid database handle", ErrorKind::Invalid);

    return impl_->put(txn.implRef(), key, value, flags);
}

bool
Dbi::delRaw(Txn &txn, std::string_view key)
{
    if (!impl_)
        throw Error("Invalid database handle", ErrorKind::Invalid);

    return impl_->del(txn.implRef(), key);
}

bool
Dbi::delRaw(Txn &txn, std::string_view key, std::string_view value)
{
    if (!impl_)
        throw Error("Invalid database handle", ErrorKind::Invalid);

    return impl_->del(txn.implRef(), key, value);
}

bool
Dbi::drop(Txn &txn, bool del)
{
    if (!impl_)
        throw Error("Invalid database handle", ErrorKind::Invalid);

    return impl_->drop(txn.implRef(), del);
}

std::size_t
Dbi::size(Txn &txn)
{
    if (!impl_)
        throw Error("Invalid database handle", ErrorKind::Invalid);

    return impl_->size(txn.implRef());
}

detail::DbiImpl &
Dbi::implRef()
{
    return *impl_;
}

const detail::DbiImpl &
Dbi::implRef() const
{
    return *impl_;
}

Cursor::Cursor(std::unique_ptr<detail::CursorImpl> impl)
  : impl_(std::move(impl))
{
}

Cursor::~Cursor()                  = default;
Cursor::Cursor(Cursor &&) noexcept = default;
Cursor &
Cursor::operator=(Cursor &&) noexcept = default;

Cursor
Cursor::open(Txn &txn, Dbi dbi)
{
    if (!detail::dbiImpl(dbi))
        throw Error("Invalid database handle", ErrorKind::Invalid);

    return Cursor{detail::dbiImpl(dbi)->openCursor(txn.implRef())};
}

bool
Cursor::getRaw(std::string_view &key, std::string_view &value, CursorOp op)
{
    if (!impl_)
        throw Error("Invalid database cursor", ErrorKind::Invalid);

    return impl_->get(key, value, op);
}

bool
Cursor::getRaw(std::string_view &key, CursorOp op)
{
    if (!impl_)
        throw Error("Invalid database cursor", ErrorKind::Invalid);

    return impl_->get(key, op);
}

bool
Cursor::putRaw(std::string_view key, std::string_view value, PutFlags flags)
{
    if (!impl_)
        throw Error("Invalid database cursor", ErrorKind::Invalid);

    return impl_->put(key, value, flags);
}

bool
Cursor::del(unsigned flags)
{
    return delRaw(flags);
}

bool
Cursor::delRaw(unsigned flags)
{
    if (!impl_)
        throw Error("Invalid database cursor", ErrorKind::Invalid);

    return impl_->del(flags);
}

void
Cursor::close()
{
    closeRaw();
}

void
Cursor::closeRaw()
{
    if (!impl_)
        throw Error("Invalid database cursor", ErrorKind::Invalid);

    impl_->close();
}

} // namespace db

namespace db::detail {

TxnImpl *
txnImpl(Txn &txn) noexcept
{
    return txn.impl_.get();
}

const TxnImpl *
txnImpl(const Txn &txn) noexcept
{
    return txn.impl_.get();
}

DbiImpl *
dbiImpl(Dbi &dbi) noexcept
{
    return dbi.impl_.get();
}

const DbiImpl *
dbiImpl(const Dbi &dbi) noexcept
{
    return dbi.impl_.get();
}

} // namespace db::detail
