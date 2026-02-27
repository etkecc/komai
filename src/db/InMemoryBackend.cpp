// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "db/InMemoryBackend.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "db/Catalog.h"
#include "db/DbTypes.h"
#include "db/Internal.h"

namespace {

constexpr std::size_t kIntegerKeySize = sizeof(std::uint64_t);

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

struct KeyLess
{
    bool integerKey = false;

    bool operator()(const std::string &lhs, const std::string &rhs) const
    {
        if (integerKey && lhs.size() == kIntegerKeySize && rhs.size() == kIntegerKeySize)
            return readIntegerKey(lhs) < readIntegerKey(rhs);
        return lhs < rhs;
    }
};

int
compareDupValues(db::DupsortComparator comparator, std::string_view lhs, std::string_view rhs)
{
    switch (comparator) {
    case db::DupsortComparator::StateKey:
        return stateKeyFromCompositeValue(lhs).compare(stateKeyFromCompositeValue(rhs));
    }

    return lhs.compare(rhs);
}

struct InMemoryDatabase
{
    explicit InMemoryDatabase(db::StoreFlags flags = db::StoreFlags::None)
      : flags(flags)
      , records(KeyLess{db::hasFlag(flags, db::StoreFlags::IntegerKey)})
    {
    }

    db::StoreFlags flags;
    db::DupsortComparator dupsortComparator = db::DupsortComparator::StateKey;
    bool hasDupsortComparator               = false;

    std::map<std::string, std::vector<std::string>, KeyLess> records;
};

struct Snapshot
{
    std::map<std::string, InMemoryDatabase> dbs;
};

struct BackendState
{
    mutable std::mutex mutex;
    Snapshot committed;

    bool open                 = false;
    std::size_t mapSize       = 0;
    unsigned maxStores        = 0;
    db::Durability durability = db::Durability::Relaxed;
};

class InMemoryTxnImpl final : public db::detail::TxnImpl
{
public:
    InMemoryTxnImpl(BackendState *backend, Snapshot snapshot, bool readOnly)
      : backend_(backend)
      , snapshot_(std::move(snapshot))
      , readOnly_(readOnly)
    {
    }

    void commit() override;
    void abort() override;
    void renew() override;
    void reset() noexcept override { active_ = false; }

    Snapshot &mutableSnapshot();
    const Snapshot &snapshot() const;
    BackendState *backend() const noexcept { return backend_; }
    bool isReadOnly() const noexcept { return readOnly_; }

private:
    BackendState *backend_ = nullptr;
    Snapshot snapshot_;
    bool readOnly_ = false;
    bool active_   = true;
    bool done_     = false;
};

class InMemoryDbiImpl;

class InMemoryCursorImpl final : public db::detail::CursorImpl
{
public:
    InMemoryCursorImpl(InMemoryDbiImpl &dbi, InMemoryTxnImpl &txn)
      : dbi_(dbi)
      , txn_(txn)
    {
    }

    bool get(std::string_view &key, std::string_view &value, db::CursorOp op) override;
    bool get(std::string_view &key, db::CursorOp op) override;
    bool put(std::string_view key, std::string_view value, db::PutFlags flags) override;
    bool del(unsigned flags) override;
    void close() override { closed_ = true; }

private:
    struct Item
    {
        std::string key;
        std::string value;
    };

    enum class Direction
    {
        None,
        Next,
        Prev,
    };

    std::vector<Item> loadItems() const;
    int compareKey(std::string_view lhs, std::string_view rhs) const;
    int findByOp(std::vector<Item> &items,
                 db::CursorOp op,
                 std::string_view key,
                 std::string_view value) const;
    bool getImpl(std::string_view &key, std::string_view &value, db::CursorOp op, bool withValue);

    InMemoryDbiImpl &dbi_;
    InMemoryTxnImpl &txn_;
    bool closed_      = false;
    bool hasCursor_   = false;
    int index_        = -1;
    bool afterDelete_ = false;
    int deletedIndex_ = -1;
    std::string keyBuffer_;
    std::string valueBuffer_;
    std::string deletedKey_;
    Direction lastDirection_ = Direction::None;
};

class InMemoryDbiImpl final : public db::detail::DbiImpl
{
public:
    InMemoryDbiImpl(BackendState *backend, std::string name, db::StoreFlags openFlags)
      : backend_(backend)
      , name_(std::move(name))
      , openFlags_(openFlags)
    {
    }

    bool get(db::detail::TxnImpl &txn, std::string_view key, std::string_view &value) override;
    bool put(db::detail::TxnImpl &txn,
             std::string_view key,
             std::string_view value,
             db::PutFlags flags) override;
    bool del(db::detail::TxnImpl &txn, std::string_view key) override;
    bool del(db::detail::TxnImpl &txn, std::string_view key, std::string_view value) override;
    bool drop(db::detail::TxnImpl &txn, bool del) override;
    std::size_t size(db::detail::TxnImpl &txn) override;
    std::unique_ptr<db::detail::CursorImpl> openCursor(db::detail::TxnImpl &txn) override;

    BackendState *backend() const noexcept { return backend_; }
    const std::string &name() const noexcept { return name_; }
    db::StoreFlags openFlags() const noexcept { return openFlags_; }

    InMemoryDatabase *lookupMutable(InMemoryTxnImpl &txn, bool createIfMissing);
    const InMemoryDatabase *lookup(const InMemoryTxnImpl &txn) const;

private:
    void sortDupValues(InMemoryDatabase &db, std::vector<std::string> &values) const;

    BackendState *backend_;
    std::string name_;
    db::StoreFlags openFlags_;
};

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

std::vector<InMemoryCursorImpl::Item>
InMemoryCursorImpl::loadItems() const
{
    std::vector<Item> items;

    const auto *db = dbi_.lookup(txn_);
    if (!db)
        return items;

    for (const auto &[key, values] : db->records) {
        if (values.empty()) {
            items.push_back(Item{key, ""});
            continue;
        }

        for (const auto &value : values)
            items.push_back(Item{key, value});
    }

    return items;
}

int
InMemoryCursorImpl::compareKey(std::string_view lhs, std::string_view rhs) const
{
    const auto *db = dbi_.lookup(txn_);
    if (!db)
        return lhs.compare(rhs);

    const auto less = db->records.key_comp();
    const auto l    = std::string(lhs);
    const auto r    = std::string(rhs);
    if (less(l, r))
        return -1;
    if (less(r, l))
        return 1;
    return 0;
}

int
InMemoryCursorImpl::findByOp(std::vector<Item> &items,
                             db::CursorOp op,
                             std::string_view key,
                             std::string_view value) const
{
    if (items.empty())
        return -1;

    auto firstForKey = [&](std::string_view wanted) {
        for (std::size_t i = 0; i < items.size(); ++i) {
            if (compareKey(items[i].key, wanted) == 0)
                return static_cast<int>(i);
        }
        return -1;
    };
    auto firstForRange = [&](std::string_view wanted) {
        for (std::size_t i = 0; i < items.size(); ++i) {
            if (compareKey(items[i].key, wanted) >= 0)
                return static_cast<int>(i);
        }
        return -1;
    };

    switch (op) {
    case db::CursorOp::First:
        return 0;
    case db::CursorOp::Last:
        return static_cast<int>(items.size() - 1);
    case db::CursorOp::Set:
    case db::CursorOp::FirstDup:
        return firstForKey(key);
    case db::CursorOp::SetRange:
        return firstForRange(key);
    case db::CursorOp::GetBoth:
        for (std::size_t i = 0; i < items.size(); ++i) {
            if (compareKey(items[i].key, key) == 0 && items[i].value == value)
                return static_cast<int>(i);
        }
        return -1;
    case db::CursorOp::Next:
        if (afterDelete_)
            return deletedIndex_;
        if (!hasCursor_)
            return 0;
        return index_ + 1;
    case db::CursorOp::Prev:
        if (afterDelete_)
            return deletedIndex_ - 1;
        if (!hasCursor_)
            return static_cast<int>(items.size() - 1);
        return index_ - 1;
    case db::CursorOp::NextDup: {
        if (!hasCursor_ || index_ < 0 || index_ >= static_cast<int>(items.size()))
            return -1;
        const auto currentKey = items[index_].key;
        for (int i = index_ + 1; i < static_cast<int>(items.size()); ++i) {
            if (compareKey(items[i].key, currentKey) != 0)
                break;
            return i;
        }
        return -1;
    }
    case db::CursorOp::NextNoDup: {
        if (!hasCursor_)
            return 0;
        if (index_ < 0 || index_ >= static_cast<int>(items.size()))
            return -1;
        const auto currentKey = afterDelete_ ? deletedKey_ : items[index_].key;
        for (int i = afterDelete_ ? deletedIndex_ : index_ + 1; i < static_cast<int>(items.size());
             ++i) {
            if (compareKey(items[i].key, currentKey) != 0)
                return i;
        }
        return -1;
    }
    }

    return -1;
}

bool
InMemoryCursorImpl::getImpl(std::string_view &key,
                            std::string_view &value,
                            db::CursorOp op,
                            bool withValue)
{
    if (closed_)
        throw db::Error("Cursor is closed", db::ErrorKind::Invalid);

    auto items      = loadItems();
    const int found = findByOp(items, op, key, value);

    afterDelete_  = false;
    deletedIndex_ = -1;

    if (found < 0 || found >= static_cast<int>(items.size())) {
        hasCursor_ = false;
        return false;
    }

    index_     = found;
    hasCursor_ = true;

    keyBuffer_ = items[found].key;
    key        = keyBuffer_;
    if (withValue) {
        valueBuffer_ = items[found].value;
        value        = valueBuffer_;
    }

    if (op == db::CursorOp::Next || op == db::CursorOp::NextDup || op == db::CursorOp::NextNoDup)
        lastDirection_ = Direction::Next;
    else if (op == db::CursorOp::Prev)
        lastDirection_ = Direction::Prev;
    else
        lastDirection_ = Direction::None;

    return true;
}

bool
InMemoryCursorImpl::get(std::string_view &key, std::string_view &value, db::CursorOp op)
{
    return getImpl(key, value, op, true);
}

bool
InMemoryCursorImpl::get(std::string_view &key, db::CursorOp op)
{
    std::string_view ignored;
    return getImpl(key, ignored, op, false);
}

bool
InMemoryCursorImpl::put(std::string_view key, std::string_view value, db::PutFlags flags)
{
    if (closed_)
        throw db::Error("Cursor is closed", db::ErrorKind::Invalid);

    return dbi_.put(txn_, key, value, flags);
}

bool
InMemoryCursorImpl::del(unsigned /*flags*/)
{
    if (closed_)
        throw db::Error("Cursor is closed", db::ErrorKind::Invalid);
    if (!hasCursor_)
        return false;

    auto items = loadItems();
    if (index_ < 0 || index_ >= static_cast<int>(items.size()))
        return false;

    const auto key = items[index_].key;
    const auto val = items[index_].value;

    auto *db = dbi_.lookupMutable(txn_, false);
    if (!db)
        return false;

    auto it = db->records.find(key);
    if (it == db->records.end())
        return false;

    if (!db::hasFlag(db->flags, db::StoreFlags::DupSort)) {
        db->records.erase(it);
    } else {
        auto &values = it->second;
        auto vit     = std::find(values.begin(), values.end(), val);
        if (vit == values.end())
            return false;
        values.erase(vit);
        if (values.empty())
            db->records.erase(it);
    }

    deletedKey_   = key;
    deletedIndex_ = index_;
    afterDelete_  = true;

    if (lastDirection_ == Direction::Next)
        index_ -= 1;

    return true;
}

} // namespace

namespace db {

struct InMemoryBackend::Impl
{
    BackendState state;
};

InMemoryBackend::InMemoryBackend()
  : impl_(std::make_unique<Impl>())
{
}

InMemoryBackend::~InMemoryBackend() = default;

void
InMemoryBackend::open(std::string_view /*directory*/, const BackendOptions &options)
{
    std::scoped_lock lock(impl_->state.mutex);

    impl_->state.committed  = {};
    impl_->state.open       = true;
    impl_->state.mapSize    = options.mapSizeBytes;
    impl_->state.maxStores  = options.maxStores;
    impl_->state.durability = options.durability;
}

void
InMemoryBackend::close() noexcept
{
    std::scoped_lock lock(impl_->state.mutex);
    impl_->state.committed = {};
    impl_->state.open      = false;
}

bool
InMemoryBackend::isOpen() const noexcept
{
    std::scoped_lock lock(impl_->state.mutex);
    return impl_->state.open;
}

Txn
InMemoryBackend::beginTxn(Txn *parent, TxnFlags flags)
{
    if (!isOpen())
        throw Error("In-memory backend is not open", ErrorKind::Invalid);
    if (parent)
        throw Error("Nested in-memory transactions are not implemented", ErrorKind::Invalid);

    const auto readOnly = hasFlag(flags, TxnFlags::ReadOnly);

    std::scoped_lock lock(impl_->state.mutex);
    return Txn{std::make_shared<InMemoryTxnImpl>(&impl_->state, impl_->state.committed, readOnly)};
}

bool
InMemoryBackend::ownsTxn(const Txn &txn) const noexcept
{
    const auto *implTxn = maybeTxn(detail::txnImpl(txn));
    return implTxn && implTxn->backend() == &impl_->state;
}

Store
InMemoryBackend::openStore(Txn &txn, std::string_view name, const StoreOpenOptions &options)
{
    if (!isOpen())
        throw Error("In-memory backend is not open", ErrorKind::Invalid);
    if (!ownsTxn(txn))
        throw Error("Transaction does not belong to in-memory backend", ErrorKind::Invalid);
    if (name.empty())
        throw Error("Database name must not be empty", ErrorKind::Invalid);

    const std::string dbName{name};
    const auto flags = options.flags;

    auto &inTxn       = requireTxn(*detail::txnImpl(txn));
    const bool exists = inTxn.snapshot().dbs.find(dbName) != inTxn.snapshot().dbs.end();
    if (!exists) {
        if (!hasFlag(flags, StoreFlags::Create) || inTxn.isReadOnly())
            throw Error("In-memory database does not exist", ErrorKind::Invalid);

        auto &snapshot = inTxn.mutableSnapshot();
        if (impl_->state.maxStores > 0 && snapshot.dbs.size() >= impl_->state.maxStores)
            throw Error("Maximum number of in-memory databases reached", ErrorKind::DbsFull);
        snapshot.dbs.emplace(dbName, InMemoryDatabase{flags});
    }

    if (options.dupsortComparator.has_value()) {
        const auto &snapshot = inTxn.snapshot();
        auto it              = snapshot.dbs.find(dbName);
        if (it == snapshot.dbs.end())
            throw Error("In-memory database does not exist", ErrorKind::Invalid);

        const auto &db = it->second;
        if (!hasFlag(db.flags, StoreFlags::DupSort))
            throw Error("dupsort comparator requires DupSort database flag", ErrorKind::Invalid);

        if (db.hasDupsortComparator) {
            if (db.dupsortComparator != *options.dupsortComparator)
                throw Error("in-memory dupsort comparator mismatch", ErrorKind::Invalid);
        } else {
            auto &mutableDb                = inTxn.mutableSnapshot().dbs.at(dbName);
            mutableDb.dupsortComparator    = *options.dupsortComparator;
            mutableDb.hasDupsortComparator = true;

            for (auto &[_, values] : mutableDb.records)
                std::sort(values.begin(),
                          values.end(),
                          [&mutableDb](const std::string &lhs, const std::string &rhs) {
                              const auto cmp =
                                compareDupValues(mutableDb.dupsortComparator, lhs, rhs);
                              if (cmp != 0)
                                  return cmp < 0;
                              return lhs < rhs;
                          });
        }
    }

    return Store{std::make_shared<InMemoryDbiImpl>(&impl_->state, dbName, flags)};
}

bool
InMemoryBackend::supports(StoreCapability capability) const noexcept
{
    switch (capability) {
    case StoreCapability::DuplicateKeys:
    case StoreCapability::IntegerKeys:
    case StoreCapability::PrefixScan:
        return true;
    case StoreCapability::None:
    default:
        return capability == StoreCapability::None;
    }
}

std::vector<std::string>
InMemoryBackend::listStoreNames(Txn &txn)
{
    if (!isOpen())
        throw Error("In-memory backend is not open", ErrorKind::Invalid);
    if (!ownsTxn(txn))
        throw Error("Transaction does not belong to in-memory backend", ErrorKind::Invalid);

    const auto &snapshot = requireTxn(*detail::txnImpl(txn)).snapshot();
    std::vector<std::string> names;
    names.reserve(snapshot.dbs.size());
    for (const auto &[name, _] : snapshot.dbs)
        names.push_back(name);
    return names;
}

std::optional<std::size_t>
InMemoryBackend::mapSizeBytes() const noexcept
{
    if (!isOpen())
        return std::nullopt;
    return impl_->state.mapSize;
}

} // namespace db
