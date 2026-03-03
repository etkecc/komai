// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "db/Backend.h"
#include "db/Internal.h"

namespace db::inmemory {

constexpr std::size_t kIntegerKeySize = sizeof(std::uint64_t);

std::uint64_t
readIntegerKey(std::string_view key);
std::string_view
stateKeyFromCompositeValue(std::string_view value);
int
compareDupValues(db::DupsortComparator comparator, std::string_view lhs, std::string_view rhs);

struct KeyLess
{
    bool integerKey = false;

    bool operator()(const std::string &lhs, const std::string &rhs) const;
};

struct InMemoryDatabase
{
    explicit InMemoryDatabase(db::StoreFlags flags = db::StoreFlags::None);

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
    InMemoryTxnImpl(BackendState *backend, Snapshot snapshot, bool readOnly);

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
    InMemoryCursorImpl(InMemoryDbiImpl &dbi, InMemoryTxnImpl &txn);

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
    InMemoryDbiImpl(BackendState *backend, std::string name, db::StoreFlags openFlags);

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
requireTxn(db::detail::TxnImpl &txn);
const InMemoryTxnImpl *
maybeTxn(const db::detail::TxnImpl *txn) noexcept;

} // namespace db::inmemory
