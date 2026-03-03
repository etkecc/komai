// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "db/inmemory/InMemoryBackendInternal.h"

#include <algorithm>
#include <cstring>
#include <utility>

#include "db/Catalog.h"

namespace db::inmemory {

std::uint64_t
readIntegerKey(std::string_view key)
{
    std::uint64_t value = 0;
    std::memcpy(&value, key.data(), std::min(key.size(), sizeof(value)));
    return value;
}

std::string_view
stateKeyFromCompositeValue(std::string_view value)
{
    return db::catalog::splitStateEventIndexValue(value).first;
}

bool
KeyLess::operator()(const std::string &lhs, const std::string &rhs) const
{
    if (integerKey && lhs.size() == kIntegerKeySize && rhs.size() == kIntegerKeySize)
        return readIntegerKey(lhs) < readIntegerKey(rhs);
    return lhs < rhs;
}

int
compareDupValues(db::DupsortComparator comparator, std::string_view lhs, std::string_view rhs)
{
    switch (comparator) {
    case db::DupsortComparator::StateKey:
        return stateKeyFromCompositeValue(lhs).compare(stateKeyFromCompositeValue(rhs));
    }

    return lhs.compare(rhs);
}

InMemoryDatabase::InMemoryDatabase(db::StoreFlags flags)
  : flags(flags)
  , records(KeyLess{db::hasFlag(flags, db::StoreFlags::IntegerKey)})
{
}

InMemoryTxnImpl::InMemoryTxnImpl(BackendState *backend, Snapshot snapshot, bool readOnly)
  : backend_(backend)
  , snapshot_(std::move(snapshot))
  , readOnly_(readOnly)
{
}

InMemoryTxnImpl &
requireTxn(db::detail::TxnImpl &txn)
{
    auto *impl = dynamic_cast<InMemoryTxnImpl *>(&txn);
    if (!impl)
        throw db::Error("Database backend mismatch for transaction", db::ErrorKind::Invalid);
    return *impl;
}

const InMemoryTxnImpl *
maybeTxn(const db::detail::TxnImpl *txn) noexcept
{
    return dynamic_cast<const InMemoryTxnImpl *>(txn);
}

void
InMemoryTxnImpl::commit()
{
    if (done_)
        return;
    if (readOnly_) {
        done_   = true;
        active_ = false;
        return;
    }

    if (!active_)
        throw db::Error("Cannot commit inactive in-memory transaction", db::ErrorKind::Invalid);

    std::scoped_lock lock(backend_->mutex);
    backend_->committed = snapshot_;
    done_               = true;
    active_             = false;
}

void
InMemoryTxnImpl::abort()
{
    done_   = true;
    active_ = false;
}

void
InMemoryTxnImpl::renew()
{
    if (!readOnly_)
        throw db::Error("Cannot renew read-write in-memory transaction", db::ErrorKind::Invalid);
    if (done_)
        throw db::Error("Cannot renew closed in-memory transaction", db::ErrorKind::Invalid);

    std::scoped_lock lock(backend_->mutex);
    snapshot_ = backend_->committed;
    active_   = true;
}

Snapshot &
InMemoryTxnImpl::mutableSnapshot()
{
    if (readOnly_)
        throw db::Error("Cannot write through read-only in-memory transaction",
                        db::ErrorKind::Invalid);
    if (done_ || !active_)
        throw db::Error("Inactive in-memory transaction", db::ErrorKind::Invalid);
    return snapshot_;
}

const Snapshot &
InMemoryTxnImpl::snapshot() const
{
    if (done_ || !active_)
        throw db::Error("Inactive in-memory transaction", db::ErrorKind::Invalid);
    return snapshot_;
}

InMemoryDbiImpl::InMemoryDbiImpl(BackendState *backend, std::string name, db::StoreFlags openFlags)
  : backend_(backend)
  , name_(std::move(name))
  , openFlags_(openFlags)
{
}

void
InMemoryDbiImpl::sortDupValues(InMemoryDatabase &db, std::vector<std::string> &values) const
{
    if (!db::hasFlag(db.flags, db::StoreFlags::DupSort))
        return;

    std::sort(values.begin(), values.end(), [&db](const std::string &lhs, const std::string &rhs) {
        const auto cmp = db.hasDupsortComparator ? compareDupValues(db.dupsortComparator, lhs, rhs)
                                                 : lhs.compare(rhs);
        if (cmp != 0)
            return cmp < 0;
        return lhs < rhs;
    });
}

InMemoryDatabase *
InMemoryDbiImpl::lookupMutable(InMemoryTxnImpl &txn, bool createIfMissing)
{
    auto &snapshot = txn.mutableSnapshot();
    auto it        = snapshot.dbs.find(name_);
    if (it != snapshot.dbs.end())
        return &it->second;

    if (!createIfMissing)
        return nullptr;

    if (backend_->maxStores > 0 && snapshot.dbs.size() >= backend_->maxStores)
        throw db::Error("Maximum number of in-memory databases reached", db::ErrorKind::DbsFull);

    auto [inserted, _] = snapshot.dbs.emplace(name_, InMemoryDatabase{openFlags_});
    return &inserted->second;
}

const InMemoryDatabase *
InMemoryDbiImpl::lookup(const InMemoryTxnImpl &txn) const
{
    const auto &snapshot = txn.snapshot();
    auto it              = snapshot.dbs.find(name_);
    if (it == snapshot.dbs.end())
        return nullptr;
    return &it->second;
}

bool
InMemoryDbiImpl::get(db::detail::TxnImpl &txn, std::string_view key, std::string_view &value)
{
    const auto *db = lookup(requireTxn(txn));
    if (!db)
        return false;

    auto it = db->records.find(std::string(key));
    if (it == db->records.end() || it->second.empty())
        return false;

    value = it->second.front();
    return true;
}

bool
InMemoryDbiImpl::put(db::detail::TxnImpl &txn,
                     std::string_view key,
                     std::string_view value,
                     db::PutFlags /*flags*/)
{
    auto &inTxn = requireTxn(txn);
    auto *db    = lookupMutable(inTxn, db::hasFlag(openFlags_, db::StoreFlags::Create));
    if (!db)
        throw db::Error("In-memory database does not exist", db::ErrorKind::Invalid);

    auto &values = db->records[std::string(key)];
    if (db::hasFlag(db->flags, db::StoreFlags::DupSort)) {
        values.emplace_back(value);
        sortDupValues(*db, values);
    } else {
        values.assign(1, std::string(value));
    }

    return true;
}

bool
InMemoryDbiImpl::del(db::detail::TxnImpl &txn, std::string_view key)
{
    auto *db = lookupMutable(requireTxn(txn), false);
    if (!db)
        return false;

    return db->records.erase(std::string(key)) > 0;
}

bool
InMemoryDbiImpl::del(db::detail::TxnImpl &txn, std::string_view key, std::string_view value)
{
    auto *db = lookupMutable(requireTxn(txn), false);
    if (!db)
        return false;

    auto it = db->records.find(std::string(key));
    if (it == db->records.end())
        return false;

    if (!db::hasFlag(db->flags, db::StoreFlags::DupSort)) {
        if (it->second.empty() || it->second.front() != value)
            return false;
        db->records.erase(it);
        return true;
    }

    auto &values       = it->second;
    const auto oldSize = values.size();
    values.erase(std::remove(values.begin(), values.end(), std::string(value)), values.end());

    if (values.empty())
        db->records.erase(it);

    return values.size() != oldSize;
}

bool
InMemoryDbiImpl::drop(db::detail::TxnImpl &txn, bool del)
{
    auto &snapshot = requireTxn(txn).mutableSnapshot();
    auto it        = snapshot.dbs.find(name_);
    if (it == snapshot.dbs.end())
        return false;

    if (del)
        snapshot.dbs.erase(it);
    else
        it->second.records.clear();

    return true;
}

std::size_t
InMemoryDbiImpl::size(db::detail::TxnImpl &txn)
{
    const auto *db = lookup(requireTxn(txn));
    if (!db)
        return 0;

    if (!db::hasFlag(db->flags, db::StoreFlags::DupSort))
        return db->records.size();

    std::size_t total = 0;
    for (const auto &[_, values] : db->records)
        total += values.size();
    return total;
}

std::unique_ptr<db::detail::CursorImpl>
InMemoryDbiImpl::openCursor(db::detail::TxnImpl &txn)
{
    return std::make_unique<InMemoryCursorImpl>(*this, requireTxn(txn));
}

} // namespace db::inmemory
