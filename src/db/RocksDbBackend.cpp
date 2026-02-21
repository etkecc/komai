// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "db/RocksDbBackend.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <mutex>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>
#include <rocksdb/db.h>
#include <rocksdb/iterator.h>
#include <rocksdb/options.h>
#include <rocksdb/status.h>
#include <rocksdb/write_batch.h>

#include "db/Catalog.h"
#include "db/DbTypes.h"
#include "db/Internal.h"
#include "db/Json.h"

namespace {

constexpr char kStorePrefixSeparator = '\0';
constexpr std::string_view kStoreMetaPrefix = "__komai_db_meta/";
constexpr std::string_view kStoreDataPrefix = "__komai_db_data/";
constexpr std::size_t kIntegerKeySize      = sizeof(std::uint64_t);

struct StoreConfig
{
    db::StoreFlags flags = db::StoreFlags::None;
    bool hasComparator   = false;
    db::DupsortComparator comparator = db::DupsortComparator::StateKey;
};

std::uint64_t
readIntegerKey(std::string_view key)
{
    std::uint64_t value = 0;
    if (key.size() >= kIntegerKeySize)
        std::memcpy(&value, key.data(), kIntegerKeySize);
    else
        std::memcpy(&value, key.data(), key.size());

    return value;
}

std::string
encodeIntegerKey(std::string_view key)
{
    return std::string(key);
}

std::string
decodeIntegerKey(std::string_view key)
{
    return std::string(key);
}

std::string_view
stateKeyFromCompositeValue(std::string_view value)
{
    return db::catalog::splitStateEventIndexValue(value).first;
}

std::string
stateKeyFromLegacyJson(std::string_view value)
{
    nlohmann::json parsed;
    if (!db::parseJsonValue(value, parsed))
        return {};

    return parsed.value("key", "");
}

int
compareStateKey(std::string_view lhs, std::string_view rhs)
{
    return stateKeyFromCompositeValue(lhs).compare(stateKeyFromCompositeValue(rhs));
}

int
compareLegacyStateByKeyJson(std::string_view lhs, std::string_view rhs)
{
    return stateKeyFromLegacyJson(lhs).compare(stateKeyFromLegacyJson(rhs));
}

int
compareDupValues(db::DupsortComparator comparator, std::string_view lhs, std::string_view rhs)
{
    switch (comparator) {
    case db::DupsortComparator::StateKey:
        return compareStateKey(lhs, rhs);
    case db::DupsortComparator::LegacyStateByKeyJson:
        return compareLegacyStateByKeyJson(lhs, rhs);
    }

    return std::string_view(lhs).compare(rhs);
}

std::string
encodeStorePrefix(std::string_view dbName)
{
    std::string prefix(kStoreDataPrefix);
    prefix.push_back(kStorePrefixSeparator);
    return prefix + std::string(dbName);
}

std::string
encodeStoreKey(std::string_view dbName, std::string_view key, bool integerKey)
{
    std::string dataKey = encodeStorePrefix(dbName);
    dataKey.append(integerKey ? encodeIntegerKey(key) : key);
    return dataKey;
}

std::string
encodeMetaKey(std::string_view dbName)
{
    std::string key{kStoreMetaPrefix};
    key.append(dbName);
    return key;
}

std::string
userStoreKeyFromDataKey(std::string_view dataKey, std::string_view dbName, bool integerKey)
{
    const auto prefix = encodeStorePrefix(dbName);
    if (!dataKey.starts_with(prefix))
        return {};

    const auto userKey = dataKey.substr(prefix.size());
    return integerKey ? decodeIntegerKey(userKey) : std::string(userKey);
}

std::string
encodeValueList(const std::vector<std::string_view> &values)
{
    std::size_t estimated = 0;
    for (const auto &value : values)
        estimated += sizeof(std::uint32_t) + value.size();

    std::string encoded;
    encoded.reserve(estimated);
    for (const auto &value : values) {
        const auto length = static_cast<std::uint32_t>(value.size());
        encoded.push_back(static_cast<char>((length >> 24) & 0xff));
        encoded.push_back(static_cast<char>((length >> 16) & 0xff));
        encoded.push_back(static_cast<char>((length >> 8) & 0xff));
        encoded.push_back(static_cast<char>(length & 0xff));
        encoded.append(value.data(), value.size());
    }

    return encoded;
}

bool
decodeValueList(std::string_view raw, std::vector<std::string> &values)
{
    values.clear();
    for (std::size_t offset = 0; offset < raw.size();) {
        if (offset + sizeof(std::uint32_t) > raw.size())
            return false;

        const auto length = (static_cast<std::uint32_t>(static_cast<unsigned char>(raw[offset])) << 24) |
                           (static_cast<std::uint32_t>(static_cast<unsigned char>(raw[offset + 1])) << 16) |
                           (static_cast<std::uint32_t>(static_cast<unsigned char>(raw[offset + 2])) << 8) |
                           static_cast<std::uint32_t>(static_cast<unsigned char>(raw[offset + 3]));
        offset += sizeof(std::uint32_t);
        if (offset + length > raw.size())
            return false;

        values.emplace_back(raw.substr(offset, length));
        offset += length;
    }

    return true;
}

void
throwIfRocksError(const rocksdb::Status &status, const char *context)
{
    if (status.ok())
        return;

    auto kind = db::ErrorKind::Unknown;
    if (status.IsNoSpace())
        kind = db::ErrorKind::MapFull;
    else if (status.IsNotFound())
        kind = db::ErrorKind::Invalid;
    else if (status.IsInvalidArgument() || status.IsCorruption())
        kind = db::ErrorKind::Invalid;

    throw db::Error(std::string(context) + ": " + status.ToString(), kind);
}

db::Error
backendMismatchError(const char *object)
{
    return db::Error(std::string("Database backend mismatch for ") + object, db::ErrorKind::Invalid);
}

class RocksDbTxnImpl final : public db::detail::TxnImpl
{
public:
    RocksDbTxnImpl(std::shared_ptr<rocksdb::DB> native,
                   std::uint64_t generation,
                   bool readOnly,
                   db::Durability durability)
      : db_(native)
      , generation_(generation)
      , durability_(durability)
      , readOnly_(readOnly)
    {
        if (readOnly_ && db_) {
            const auto snapshot = db_->GetSnapshot();
            if (snapshot)
                snapshot_ = snapshot;
        }
    }

    ~RocksDbTxnImpl() override
    {
        if (snapshot_) {
            db_->ReleaseSnapshot(snapshot_);
            snapshot_ = nullptr;
        }
    }

    void
    commit() override
    {
        if (done_)
            return;

        if (readOnly_) {
            done_   = true;
            active_ = false;
            batch_.Clear();
            pendingWrites_.clear();
            readCache_.clear();
            return;
        }

        if (!active_) {
            throw db::Error("Cannot commit inactive RocksDB transaction", db::ErrorKind::Invalid);
        }

        rocksdb::WriteOptions options;
        options.sync = (durability_ == db::Durability::Durable);
        throwIfRocksError(db_->Write(options, &batch_), "Failed to commit RocksDB transaction");

        done_        = true;
        active_      = false;
        batch_.Clear();
        pendingWrites_.clear();
        readCache_.clear();
    }

    void
    abort() override
    {
        done_        = true;
        active_      = false;
        batch_.Clear();
        pendingWrites_.clear();
        readCache_.clear();
    }

    void
    renew() override
    {
        if (!readOnly_) {
            throw db::Error("Cannot renew read-write RocksDB transaction", db::ErrorKind::Invalid);
        }
        if (done_)
            throw db::Error("Cannot renew closed RocksDB transaction", db::ErrorKind::Invalid);

        if (snapshot_) {
            db_->ReleaseSnapshot(snapshot_);
            snapshot_ = nullptr;
        }

        const auto snapshot = db_->GetSnapshot();
        if (snapshot)
            snapshot_ = snapshot;

        batch_.Clear();
        pendingWrites_.clear();
        readCache_.clear();
        active_ = true;
    }

    void
    reset() noexcept override
    {
        if (readOnly_ && snapshot_) {
            db_->ReleaseSnapshot(snapshot_);
            snapshot_ = db_->GetSnapshot();
            batch_.Clear();
            pendingWrites_.clear();
            readCache_.clear();
            active_  = true;
            done_    = false;
            return;
        }

        batch_.Clear();
        pendingWrites_.clear();
        readCache_.clear();
        active_ = false;
        done_   = false;
    }

    bool
    isReadOnly() const noexcept
    {
        return readOnly_;
    }

    const rocksdb::Snapshot *
    snapshot() const noexcept
    {
        return snapshot_;
    }

    void
    detach() noexcept
    {
        if (snapshot_) {
            db_->ReleaseSnapshot(snapshot_);
            snapshot_ = nullptr;
        }
        done_   = true;
        active_ = false;
        db_.reset();
        readCache_.clear();
    }

    rocksdb::DB *
    db() const noexcept
    {
        return db_.get();
    }

    std::uint64_t
    generation() const noexcept
    {
        return generation_;
    }

    rocksdb::WriteBatch &
    batch()
    {
        if (done_)
            throw db::Error("Cannot write to inactive RocksDB transaction", db::ErrorKind::Invalid);
        return batch_;
    }

    void
    markDeleted(std::string_view key)
    {
        batch().Delete(std::string(key));
        pendingWrites_[std::string(key)] = std::nullopt;
    }

    void
    markWritten(std::string_view key, std::string_view value)
    {
        pendingWrites_[std::string(key)] = std::string(value);
    }

    bool
    isDeleted(std::string_view key) const noexcept
    {
        const auto it = pendingWrites_.find(std::string(key));
        return it != pendingWrites_.end() && !it->second.has_value();
    }

    bool
    getPending(std::string_view key, std::string &value) const
    {
        const auto it = pendingWrites_.find(std::string(key));
        if (it == pendingWrites_.end() || !it->second.has_value())
            return false;
        value = *it->second;
        return true;
    }

    bool
    getCachedRead(std::string_view key, std::string_view &value) const noexcept
    {
        const auto it = readCache_.find(std::string(key));
        if (it == readCache_.end())
            return false;

        value = it->second;
        return true;
    }

    std::string_view
    cacheRead(std::string_view key, std::string_view value)
    {
        auto &entry = readCache_[std::string(key)];
        entry        = value;
        return entry;
    }

private:
    std::shared_ptr<rocksdb::DB> db_;
    std::uint64_t generation_ = 0;
    db::Durability durability_ = db::Durability::Relaxed;
    bool readOnly_ = false;
    const rocksdb::Snapshot *snapshot_ = nullptr;
    bool done_ = false;
    bool active_ = true;
    rocksdb::WriteBatch batch_;
    std::map<std::string, std::string> readCache_;
    std::unordered_map<std::string, std::optional<std::string>> pendingWrites_;
};

bool
readFromTransaction(RocksDbTxnImpl &txn, std::string_view key, std::string &value, const char *context)
{
    if (txn.getPending(key, value))
        return true;
    if (txn.isDeleted(key))
        return false;

    rocksdb::ReadOptions options;
    options.snapshot = txn.snapshot();
    auto status      = txn.db()->Get(options, key, &value);
    if (status.ok())
        return true;
    if (status.IsNotFound())
        return false;

    throwIfRocksError(status, context);
    return false;
}

class RocksDbCursorImpl final : public db::detail::CursorImpl
{
public:
    RocksDbCursorImpl(db::detail::TxnImpl &txn, class RocksDbDbiImpl &dbi)
      : txn_(&txn)
      , dbi_(dbi)
    {
    }

    bool
    get(std::string_view &key, std::string_view &value, db::CursorOp op) override;

    bool
    get(std::string_view &key, db::CursorOp op) override;

    bool
    put(std::string_view key, std::string_view value, db::PutFlags flags) override;

    bool
    del(unsigned flags) override;

    void
    close() override
    {
        closed_ = true;
    }

private:
    struct Item
    {
        std::string key;
        std::string value;
    };

    enum class Direction {
        None,
        Next,
        Prev,
    };

    bool
    loadItems();
    bool
    getImpl(std::string_view &key, std::string_view &value, db::CursorOp op, bool withValue);
    bool
    findByOp(db::CursorOp op, std::string_view key, std::string_view value);
    int
    compareKeys(std::string_view lhs, std::string_view rhs) const;

    std::vector<Item> items_;
    bool loaded_ = false;
    bool hasCursor_ = false;
    bool closed_ = false;
    bool afterDelete_ = false;
    bool deleted_ = false;
    int index_ = -1;
    int deletedIndex_ = -1;
    Direction lastDirection_ = Direction::None;
    std::string keyBuffer_;
    std::string valueBuffer_;
    std::string deletedKey_;

    db::detail::TxnImpl *txn_ = nullptr;
    class RocksDbDbiImpl &dbi_;
};

class RocksDbDbiImpl final : public db::detail::DbiImpl
{
public:
    RocksDbDbiImpl(rocksdb::DB *db,
                    std::string name,
                    db::StoreFlags flags,
                    bool hasComparator,
                    db::DupsortComparator comparator)
      : db_(db)
      , name_(std::move(name))
      , openFlags_(flags)
      , hasComparator_(hasComparator)
      , comparator_(comparator)
    {
    }

    bool
    get(db::detail::TxnImpl &txn, std::string_view key, std::string_view &value) override;

    bool
    put(db::detail::TxnImpl &txn,
        std::string_view key,
        std::string_view value,
        db::PutFlags flags) override;

    bool
    del(db::detail::TxnImpl &txn, std::string_view key) override;

    bool
    del(db::detail::TxnImpl &txn, std::string_view key, std::string_view value) override;

    bool
    drop(db::detail::TxnImpl &txn, bool del) override;

    std::size_t
    size(db::detail::TxnImpl &txn) override;

    std::unique_ptr<db::detail::CursorImpl>
    openCursor(db::detail::TxnImpl &txn) override;

    bool
    dupSort() const noexcept
    {
        return db::hasFlag(openFlags_, db::StoreFlags::DupSort);
    }

    bool
    integerKey() const noexcept
    {
        return db::hasFlag(openFlags_, db::StoreFlags::IntegerKey);
    }

    db::StoreFlags
    flags() const noexcept
    {
        return openFlags_;
    }

    const std::string &
    name() const noexcept
    {
        return name_;
    }

    bool
    hasComparator() const noexcept
    {
        return hasComparator_;
    }

    db::DupsortComparator
    comparator() const noexcept
    {
        return comparator_;
    }

    rocksdb::DB *
    db() const noexcept
    {
        return db_;
    }

private:
    bool
    readValue(db::detail::TxnImpl &txn, std::string_view key, std::string &value) const;
    bool
    readAll(db::detail::TxnImpl &txn, std::string_view key, std::string &value) const;
    bool
    readValueList(db::detail::TxnImpl &txn, std::string_view key, std::vector<std::string> &values) const;
    void
    writeListValue(db::detail::TxnImpl &txn, std::string_view key, const std::vector<std::string_view> &values);

    std::string
    dataKey(std::string_view key) const
    {
        return encodeStoreKey(name_, key, integerKey());
    }

    rocksdb::DB *db_;
    std::string name_;
    db::StoreFlags openFlags_;
    bool hasComparator_ = false;
    db::DupsortComparator comparator_ = db::DupsortComparator::StateKey;
};

RocksDbTxnImpl &
requireRocksTxn(db::detail::TxnImpl &txn)
{
    auto *impl = dynamic_cast<RocksDbTxnImpl *>(&txn);
    if (!impl)
        throw backendMismatchError("transaction");
    return *impl;
}

const RocksDbTxnImpl *
maybeRocksTxn(const db::detail::TxnImpl *txn) noexcept
{
    return dynamic_cast<const RocksDbTxnImpl *>(txn);
}

RocksDbDbiImpl &
requireRocksDbi(db::detail::DbiImpl &dbi)
{
    auto *impl = dynamic_cast<RocksDbDbiImpl *>(&dbi);
    if (!impl)
        throw db::Error("Database backend mismatch for database handle", db::ErrorKind::Invalid);
    return *impl;
}

bool
storeExists(RocksDbTxnImpl &txn, std::string_view name)
{
    std::string raw;
    return readFromTransaction(txn, encodeMetaKey(name), raw, "Failed to read RocksDB store metadata");
}

std::optional<StoreConfig>
loadStoreConfig(db::detail::TxnImpl &txn, const std::string &name)
{
    auto &rocksTxn = requireRocksTxn(txn);
    std::string raw;
    if (!readFromTransaction(rocksTxn, encodeMetaKey(name), raw, "Failed to read RocksDB store metadata")) {
        return std::nullopt;
    }

    if (raw.size() < sizeof(unsigned char))
        throw db::Error("RocksDB store metadata is corrupt", db::ErrorKind::Invalid);

    StoreConfig config;
    config.flags = static_cast<db::StoreFlags>(static_cast<unsigned char>(raw[0]));
    config.hasComparator = raw.size() >= 2 && raw[1] != '\0';
    if (config.hasComparator) {
        if (raw.size() < 3)
            throw db::Error("RocksDB store metadata is corrupt", db::ErrorKind::Invalid);
        config.comparator =
          static_cast<db::DupsortComparator>(static_cast<unsigned char>(raw[2]));
    }

    return config;
}

void
persistStoreConfig(RocksDbTxnImpl &txn, const std::string &name, const StoreConfig &config)
{
    if (txn.isReadOnly())
        throw db::Error("Cannot modify RocksDB store metadata in read-only transaction",
                        db::ErrorKind::Invalid);

    std::string raw;
    raw.push_back(static_cast<char>(db::toUnderlying(config.flags)));
    raw.push_back(config.hasComparator ? '\x01' : '\x00');
    if (config.hasComparator)
        raw.push_back(static_cast<char>(static_cast<unsigned char>(config.comparator)));

    const auto meta = encodeMetaKey(name);
    auto &batch = txn.batch();
    batch.Put(meta, raw);
    txn.markWritten(meta, raw);
}

std::size_t
countStores(rocksdb::DB *db, const rocksdb::ReadOptions &options)
{
    std::size_t count = 0;
    const auto metaPrefix = std::string(kStoreMetaPrefix);
    std::unique_ptr<rocksdb::Iterator> iterator(db->NewIterator(options));
    for (iterator->Seek(metaPrefix); iterator->Valid(); iterator->Next()) {
        if (!iterator->key().ToString().starts_with(metaPrefix))
            break;
        ++count;
    }
    return count;
}

void
clearStoreData(db::detail::TxnImpl &txn, rocksdb::DB *db, const std::string &name, bool keepMetadata)
{
    auto &rocksTxn = requireRocksTxn(txn);
    const auto prefix = encodeStorePrefix(name);
    rocksdb::ReadOptions options;
    options.snapshot = rocksTxn.snapshot();
    std::unique_ptr<rocksdb::Iterator> iterator(db->NewIterator(options));

    for (iterator->Seek(prefix); iterator->Valid(); iterator->Next()) {
        const auto key = iterator->key().ToString();
        if (!key.starts_with(prefix))
            break;
        rocksTxn.markDeleted(key);
        rocksTxn.batch().Delete(key);
    }

    if (!keepMetadata) {
    const auto metaKey = encodeMetaKey(name);
    rocksTxn.markDeleted(metaKey);
    rocksTxn.batch().Delete(metaKey);
    }
}

bool
RocksDbDbiImpl::readValue(db::detail::TxnImpl &txn, std::string_view key, std::string &value) const
{
    auto &rocksTxn = requireRocksTxn(txn);
    const auto dataKeyValue = dataKey(key);
    return readFromTransaction(rocksTxn, dataKeyValue, value, "Failed to read RocksDB store data");
}

bool
RocksDbDbiImpl::readAll(db::detail::TxnImpl &txn, std::string_view key, std::string &value) const
{
    return readValue(txn, key, value);
}

bool
RocksDbDbiImpl::readValueList(db::detail::TxnImpl &txn,
                               std::string_view key,
                               std::vector<std::string> &values) const
{
    std::string raw;
    if (!readAll(txn, key, raw))
        return false;
    return decodeValueList(raw, values);
}

void
RocksDbDbiImpl::writeListValue(db::detail::TxnImpl &txn,
                               std::string_view key,
                               const std::vector<std::string_view> &values)
{
    auto &rocksTxn = requireRocksTxn(txn);
    const auto storeKey = dataKey(key);
    if (values.empty()) {
        rocksTxn.batch().Delete(storeKey);
        rocksTxn.markDeleted(storeKey);
        return;
    }

    const auto encoded = encodeValueList(values);
    rocksTxn.batch().Put(storeKey, encoded);
    rocksTxn.markWritten(storeKey, encoded);
}

bool
RocksDbDbiImpl::get(db::detail::TxnImpl &txn, std::string_view key, std::string_view &value)
{
    auto &rocksTxn = requireRocksTxn(txn);
    std::string raw;
    const auto dataKeyValue = dataKey(key);
    if (rocksTxn.getCachedRead(dataKeyValue, value))
        return true;

    if (db::hasFlag(openFlags_, db::StoreFlags::DupSort)) {
        std::vector<std::string> values;
        if (!readValueList(txn, key, values))
            return false;
        if (values.empty())
            return false;

        value = rocksTxn.cacheRead(dataKeyValue, values.front());
        return true;
    }

    if (!readAll(txn, key, raw))
        return false;

    value = rocksTxn.cacheRead(dataKeyValue, raw);
    return true;
}

bool
RocksDbDbiImpl::put(db::detail::TxnImpl &txn,
                     std::string_view key,
                     std::string_view value,
                     db::PutFlags /*flags*/)
{
    auto &rocksTxn = requireRocksTxn(txn);
    if (rocksTxn.isReadOnly())
        throw db::Error("Cannot write through read-only RocksDB transaction", db::ErrorKind::Invalid);

    if (!db::hasFlag(openFlags_, db::StoreFlags::DupSort)) {
        auto &batch = rocksTxn.batch();
        batch.Put(dataKey(key), value);
        rocksTxn.markWritten(dataKey(key), value);
        return true;
    }

    std::vector<std::string> values;
    readValueList(txn, key, values);
    values.emplace_back(value);

    if (hasComparator_) {
        std::sort(values.begin(), values.end(), [&](const std::string &lhs, const std::string &rhs) {
            const auto cmp = compareDupValues(comparator_, lhs, rhs);
            if (cmp != 0)
                return cmp < 0;
            return lhs < rhs;
        });
    } else {
        std::sort(values.begin(), values.end());
    }

    std::vector<std::string_view> views;
    views.reserve(values.size());
    for (auto &v : values)
        views.emplace_back(v);
    writeListValue(txn, key, views);
    return true;
}

bool
RocksDbDbiImpl::del(db::detail::TxnImpl &txn, std::string_view key)
{
    auto &rocksTxn = requireRocksTxn(txn);
    if (rocksTxn.isReadOnly())
        throw db::Error("Cannot write through read-only RocksDB transaction", db::ErrorKind::Invalid);

    std::string value;
    if (!readAll(txn, key, value))
        return false;

    rocksTxn.batch().Delete(dataKey(key));
    rocksTxn.markDeleted(dataKey(key));
    return true;
}

bool
RocksDbDbiImpl::del(db::detail::TxnImpl &txn, std::string_view key, std::string_view value)
{
    auto &rocksTxn = requireRocksTxn(txn);
    if (rocksTxn.isReadOnly())
        throw db::Error("Cannot write through read-only RocksDB transaction", db::ErrorKind::Invalid);

    if (!db::hasFlag(openFlags_, db::StoreFlags::DupSort)) {
        std::string current;
        if (!readAll(txn, key, current))
            return false;
        if (current != value)
            return false;

        rocksTxn.batch().Delete(dataKey(key));
        rocksTxn.markDeleted(dataKey(key));
        return true;
    }

    std::vector<std::string> values;
    if (!readValueList(txn, key, values))
        return false;

    const auto oldSize = values.size();
    values.erase(std::remove(values.begin(), values.end(), std::string(value)), values.end());
    if (values.empty()) {
        writeListValue(txn, key, {});
    } else {
        rocksTxn.markDeleted(dataKey(key));
        writeListValue(txn, key, std::vector<std::string_view>(values.begin(), values.end()));
    }

    return values.size() != oldSize;
}

bool
RocksDbDbiImpl::drop(db::detail::TxnImpl &txn, bool del)
{
    auto &rocksTxn = requireRocksTxn(txn);
    if (rocksTxn.isReadOnly())
        throw db::Error("Cannot drop database through read-only RocksDB transaction",
                        db::ErrorKind::Invalid);
    clearStoreData(txn, db_, name_, !del);
    return true;
}

std::size_t
RocksDbDbiImpl::size(db::detail::TxnImpl &txn)
{
    auto &rocksTxn = requireRocksTxn(txn);
    std::size_t total = 0;
    rocksdb::ReadOptions options;
    options.snapshot = rocksTxn.snapshot();
    std::unique_ptr<rocksdb::Iterator> iterator(db_->NewIterator(options));
    for (iterator->Seek(encodeStorePrefix(name_)); iterator->Valid(); iterator->Next()) {
        const auto key = iterator->key().ToString();
        if (!key.starts_with(encodeStorePrefix(name_)))
            break;

        if (db::hasFlag(openFlags_, db::StoreFlags::DupSort)) {
            std::vector<std::string> values;
            if (!decodeValueList(iterator->value().ToString(), values))
                throw db::Error("RocksDB duplicate-key payload is corrupt", db::ErrorKind::Invalid);
            total += values.size();
        } else {
            ++total;
        }
    }

    return total;
}

std::unique_ptr<db::detail::CursorImpl>
RocksDbDbiImpl::openCursor(db::detail::TxnImpl &txn)
{
    return std::make_unique<RocksDbCursorImpl>(txn, *this);
}

bool
RocksDbCursorImpl::loadItems()
{
    if (loaded_)
        return !items_.empty();

    auto &rocksTxnRef = *txn_;
    auto &rocksTxn    = requireRocksTxn(rocksTxnRef);
    const auto prefix       = encodeStorePrefix(dbi_.name());

    items_.clear();
    rocksdb::ReadOptions options;
    options.snapshot = rocksTxn.snapshot();
    std::unique_ptr<rocksdb::Iterator> iterator(dbi_.db()->NewIterator(options));
    for (iterator->Seek(prefix); iterator->Valid(); iterator->Next()) {
        const auto keySlice = iterator->key().ToString();
        if (!keySlice.starts_with(prefix))
            break;

        const auto key = userStoreKeyFromDataKey(keySlice, dbi_.name(), dbi_.integerKey());
        if (key.empty())
            continue;

        if (dbi_.dupSort()) {
            std::vector<std::string> values;
            if (!decodeValueList(iterator->value().ToString(), values)) {
                throw db::Error("RocksDB duplicate-key payload is corrupt", db::ErrorKind::Invalid);
            }
            for (auto &value : values)
                items_.push_back({key, value});
            continue;
        }

        items_.push_back({key, iterator->value().ToString()});
    }

    loaded_ = true;
    index_ = -1;
    hasCursor_ = false;
    afterDelete_ = false;
    return true;
}

int
RocksDbCursorImpl::compareKeys(std::string_view lhs, std::string_view rhs) const
{
    if (dbi_.integerKey()) {
        const auto lhsInt = readIntegerKey(lhs);
        const auto rhsInt = readIntegerKey(rhs);
        if (lhsInt < rhsInt)
            return -1;
        if (lhsInt > rhsInt)
            return 1;
        return 0;
    }

    return lhs.compare(rhs);
}

bool
RocksDbCursorImpl::findByOp(db::CursorOp op, std::string_view key, std::string_view value)
{
    const auto firstForKey = [this](std::string_view wanted) {
        for (std::size_t i = 0; i < items_.size(); ++i) {
            if (compareKeys(items_[i].key, wanted) == 0)
                return static_cast<int>(i);
        }
        return -1;
    };
    const auto firstForRange = [this](std::string_view wanted) {
        for (std::size_t i = 0; i < items_.size(); ++i) {
            if (compareKeys(items_[i].key, wanted) >= 0)
                return static_cast<int>(i);
        }
        return -1;
    };

    int target = -1;
    switch (op) {
    case db::CursorOp::First:
        target = items_.empty() ? -1 : 0;
        break;
    case db::CursorOp::Last:
        target = items_.empty() ? -1 : static_cast<int>(items_.size() - 1);
        break;
    case db::CursorOp::Set:
    case db::CursorOp::FirstDup:
        target = firstForKey(key);
        break;
    case db::CursorOp::SetRange:
        target = firstForRange(key);
        break;
    case db::CursorOp::GetBoth:
        for (std::size_t i = 0; i < items_.size(); ++i) {
            if (compareKeys(items_[i].key, key) == 0 && items_[i].value == value) {
                target = static_cast<int>(i);
                break;
            }
        }
        break;
    case db::CursorOp::Next:
        if (afterDelete_)
            target = deletedIndex_;
        else if (!hasCursor_)
            target = 0;
        else
            target = index_ + 1;
        break;
    case db::CursorOp::Prev:
        if (afterDelete_)
            target = deletedIndex_ - 1;
        else if (!hasCursor_)
            target = static_cast<int>(items_.size() - 1);
        else
            target = index_ - 1;
        break;
    case db::CursorOp::NextDup:
        if (!hasCursor_ || index_ < 0 || index_ >= static_cast<int>(items_.size()))
            break;
        for (int i = index_ + 1; i < static_cast<int>(items_.size()); ++i) {
            if (compareKeys(items_[i].key, items_[index_].key) == 0) {
                target = i;
                break;
            }
        }
        break;
    case db::CursorOp::NextNoDup:
        if (!hasCursor_ || index_ < 0 || index_ >= static_cast<int>(items_.size()))
            break;

        {
            const auto currentKey = afterDelete_ ? deletedKey_ : items_[index_].key;
            const int start = afterDelete_ ? deletedIndex_ : index_ + 1;
            for (int i = start; i < static_cast<int>(items_.size()); ++i) {
                if (compareKeys(items_[i].key, currentKey) != 0) {
                    target = i;
                    break;
                }
            }
        }
        break;
    default:
        break;
    }

    if (target < 0 || target >= static_cast<int>(items_.size()))
        return false;

    index_ = target;
    return true;
}

bool
RocksDbCursorImpl::getImpl(std::string_view &key,
                           std::string_view &value,
                           db::CursorOp op,
                           bool withValue)
{
    if (closed_)
        throw db::Error("Cursor is closed", db::ErrorKind::Invalid);

    if (!loadItems())
        return false;

    const bool found = findByOp(op, key, value);
    if (!found || index_ < 0 || index_ >= static_cast<int>(items_.size())) {
        hasCursor_ = false;
        return false;
    }

    afterDelete_ = false;
    deletedIndex_ = -1;
    deleted_      = false;
    hasCursor_    = true;
    keyBuffer_    = items_[index_].key;
    valueBuffer_  = items_[index_].value;
    key          = keyBuffer_;
    if (withValue)
        value = valueBuffer_;

    if (op == db::CursorOp::Next || op == db::CursorOp::NextDup || op == db::CursorOp::NextNoDup)
        lastDirection_ = Direction::Next;
    else if (op == db::CursorOp::Prev)
        lastDirection_ = Direction::Prev;
    else
        lastDirection_ = Direction::None;

    return true;
}

bool
RocksDbCursorImpl::get(std::string_view &key, std::string_view &value, db::CursorOp op)
{
    return getImpl(key, value, op, true);
}

bool
RocksDbCursorImpl::get(std::string_view &key, db::CursorOp op)
{
    std::string_view ignored;
    return getImpl(key, ignored, op, false);
}

bool
RocksDbCursorImpl::put(std::string_view key, std::string_view value, db::PutFlags flags)
{
    if (closed_)
        throw db::Error("Cursor is closed", db::ErrorKind::Invalid);

    return dbi_.put(*txn_, key, value, flags);
}

bool
RocksDbCursorImpl::del(unsigned /*flags*/)
{
    if (closed_)
        throw db::Error("Cursor is closed", db::ErrorKind::Invalid);
    if (!hasCursor_ || index_ < 0 || index_ >= static_cast<int>(items_.size()))
        return false;

    auto &txn      = *txn_;
    const auto key   = items_[index_].key;
    const auto value = items_[index_].value;
    auto &dbi = dbi_;

    if (!dbi.del(txn, key, value))
        return false;

    deletedKey_ = key;
    deletedIndex_ = index_;
    afterDelete_ = true;

    items_.erase(items_.begin() + index_);
    if (index_ >= static_cast<int>(items_.size()))
        index_ = static_cast<int>(items_.size()) - 1;
    else if (lastDirection_ == Direction::Next)
        index_ -= 1;

    hasCursor_ = false;
    return true;
}

} // namespace

namespace db {

class RocksDbBackend::Impl
{
public:
    std::shared_ptr<rocksdb::DB> db;
    std::mutex txnMutex;
    std::vector<std::weak_ptr<RocksDbTxnImpl>> txns;
    db::Durability durability = db::Durability::Relaxed;
    unsigned maxDbs = 0;
    std::uint64_t generation = 0;
    std::optional<std::size_t> mapSize;
    std::filesystem::path directory;
};

RocksDbBackend::RocksDbBackend()
  : impl_(std::make_unique<Impl>())
{
}

RocksDbBackend::~RocksDbBackend() = default;

bool
RocksDbBackend::isOpen() const noexcept
{
    return impl_ && impl_->db != nullptr;
}

void
RocksDbBackend::open(std::string_view directory, const BackendOptions &options)
{
    if (isOpen())
        close();

    if (directory.empty())
        throw Error("RocksDB backend requires a directory path", ErrorKind::Invalid);

    std::filesystem::create_directories(directory);

    rocksdb::Options dbOptions;
    dbOptions.create_if_missing = true;
    dbOptions.create_missing_column_families = true;
    dbOptions.compression = rocksdb::kNoCompression;
    dbOptions.use_fsync = (options.durability == Durability::Durable);

    rocksdb::DB *db = nullptr;
    throwIfRocksError(
      rocksdb::DB::Open(dbOptions, std::string(directory), &db), "Failed to open RocksDB backend");

    impl_->db = std::shared_ptr<rocksdb::DB>(db, [](rocksdb::DB *value) { delete value; });
    impl_->directory = directory;
    ++impl_->generation;
    impl_->durability = options.durability;
    impl_->maxDbs = options.maxDbs;
    impl_->mapSize = options.mapSizeBytes;
}

void
RocksDbBackend::close() noexcept
{
    if (!isOpen())
        return;

    {
        std::scoped_lock lock{impl_->txnMutex};
        for (auto &txn : impl_->txns) {
            if (const auto handle = txn.lock()) {
                handle->detach();
            }
        }
        impl_->txns.clear();
    }

    impl_->db.reset();
    impl_->directory.clear();
}

bool
RocksDbBackend::ownsTxn(const Txn &txn) const noexcept
{
    const auto *impl = maybeRocksTxn(detail::txnImpl(txn));
    return isOpen() && impl && impl->db() == impl_->db.get() &&
           impl->generation() == impl_->generation;
}

Txn
RocksDbBackend::beginTxn(Txn *parent, TxnFlags flags)
{
    if (!isOpen())
        throw Error("RocksDB backend is not open", ErrorKind::Invalid);
    if (parent)
        throw Error("Nested RocksDB transactions are not implemented", ErrorKind::Invalid);

    const bool readOnly = hasFlag(flags, TxnFlags::ReadOnly);
    auto txn = std::make_shared<RocksDbTxnImpl>(
      impl_->db, impl_->generation, readOnly, impl_->durability);
    {
        std::scoped_lock lock{impl_->txnMutex};
        impl_->txns.push_back(txn);
    }
    return Txn{txn};
}

Store
RocksDbBackend::openStore(Txn &txn, std::string_view name, const StoreOpenOptions &options)
{
    if (!isOpen())
        throw Error("RocksDB backend is not open", ErrorKind::Invalid);
    if (!ownsTxn(txn))
        throw Error("Transaction does not belong to RocksDB backend", ErrorKind::Invalid);
    if (name.empty())
        throw Error("Database name must not be empty", ErrorKind::Invalid);

    auto &rocksTxn = requireRocksTxn(*detail::txnImpl(txn));
    const auto flags = options.flags;
    const std::string dbName{name};
    const bool exists = storeExists(rocksTxn, dbName);

    if (!exists) {
        if (!hasFlag(flags, db::StoreFlags::Create) || rocksTxn.isReadOnly())
            throw Error("RocksDB store does not exist", ErrorKind::Invalid);

        if (options.dupsortComparator.has_value() && !hasFlag(flags, db::StoreFlags::DupSort))
            throw Error("dupsort comparator requires DupSort database flag", ErrorKind::Invalid);

        if (impl_->maxDbs > 0) {
            rocksdb::ReadOptions readOptions;
            readOptions.snapshot = rocksTxn.snapshot();
            if (countStores(impl_->db.get(), readOptions) >= impl_->maxDbs)
                throw Error("Maximum number of RocksDB databases reached", ErrorKind::DbsFull);
        }

        StoreConfig config;
        config.flags = flags;
        if (options.dupsortComparator.has_value()) {
            config.hasComparator = true;
            config.comparator = *options.dupsortComparator;
        }
        persistStoreConfig(rocksTxn, dbName, config);
        return Store{
          std::make_shared<RocksDbDbiImpl>(impl_->db.get(),
                                           dbName,
                                           config.flags,
                                           config.hasComparator,
                                           config.comparator)};
    }

    auto config = loadStoreConfig(rocksTxn, dbName);
    if (!config)
        throw Error("RocksDB store metadata is missing", ErrorKind::Invalid);

    if (options.dupsortComparator.has_value()) {
        if (!hasFlag(config->flags, db::StoreFlags::DupSort))
            throw Error("dupsort comparator requires DupSort database flag", ErrorKind::Invalid);

        if (config->hasComparator) {
            if (config->comparator != *options.dupsortComparator)
                throw Error("RocksDB dupsort comparator mismatch", ErrorKind::Invalid);
        } else {
            if (rocksTxn.isReadOnly())
                throw Error("Cannot set dupsort comparator from read-only transaction", ErrorKind::Invalid);
            config->hasComparator = true;
            config->comparator = *options.dupsortComparator;
            persistStoreConfig(rocksTxn, dbName, *config);
        }
    }

    return Store{std::make_shared<RocksDbDbiImpl>(
      impl_->db.get(), dbName, config->flags, config->hasComparator, config->comparator)};
}

bool
RocksDbBackend::supports(StoreCapability capability) const noexcept
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
RocksDbBackend::listStoreNames(Txn &txn)
{
    if (!detail::txnImpl(txn))
        throw Error("Invalid transaction", ErrorKind::Invalid);
    if (!ownsTxn(txn))
        throw Error("Transaction does not belong to RocksDB backend", ErrorKind::Invalid);

    auto &rocksTxn = requireRocksTxn(*detail::txnImpl(txn));
    rocksdb::ReadOptions options;
    options.snapshot = rocksTxn.snapshot();

    std::vector<std::string> names;
    const auto metaPrefix = std::string(kStoreMetaPrefix);
    std::unique_ptr<rocksdb::Iterator> iterator(impl_->db->NewIterator(options));
    for (iterator->Seek(metaPrefix); iterator->Valid(); iterator->Next()) {
        const auto key = iterator->key().ToString();
        if (!key.starts_with(metaPrefix))
            break;
        names.push_back(key.substr(metaPrefix.size()));
    }

    return names;
}

std::optional<std::size_t>
RocksDbBackend::mapSizeBytes() const noexcept
{
    return impl_->mapSize;
}

} // namespace db
