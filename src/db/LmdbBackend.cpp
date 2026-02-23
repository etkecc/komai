// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "db/LmdbBackend.h"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include "db/Catalog.h"
#include "db/DbTypes.h"
#include "db/Internal.h"
#include "db/Json.h"
#include "db/LmdbError.h"
#include "db/LmdbFlags.h"
#include "db/Scan.h"

namespace {

std::string_view
stateKeyFromCompositeValue(const MDB_val *value)
{
    auto raw = std::string_view(static_cast<const char *>(value->mv_data), value->mv_size);
    return db::catalog::splitStateEventIndexValue(raw).first;
}

int
compareStateKey(const MDB_val *a, const MDB_val *b)
{
    return stateKeyFromCompositeValue(a).compare(stateKeyFromCompositeValue(b));
}

std::string
stateKeyFromLegacyJson(const MDB_val *value)
{
    nlohmann::json parsed;
    const std::string_view raw(static_cast<const char *>(value->mv_data), value->mv_size);
    if (!db::parseJsonValue(raw, parsed)) {
        return {};
    }

    return parsed.value("key", "");
}

int
compareLegacyStateByKeyJson(const MDB_val *a, const MDB_val *b)
{
    return stateKeyFromLegacyJson(a).compare(stateKeyFromLegacyJson(b));
}

MDB_cmp_func *
dupsortComparatorFunc(db::DupsortComparator comparator)
{
    switch (comparator) {
    case db::DupsortComparator::StateKey:
        return compareStateKey;
    case db::DupsortComparator::LegacyStateByKeyJson:
        return compareLegacyStateByKeyJson;
    }

    return compareStateKey;
}

class LmdbTxnImpl final : public db::detail::TxnImpl
{
public:
    explicit LmdbTxnImpl(lmdb::txn native)
      : native_(std::move(native))
    {
    }

    void commit() override
    {
        db::translateLmdbErrors([&] { native_.commit(); });
    }
    void abort() override
    {
        db::translateLmdbErrors([&] { native_.abort(); });
    }
    void renew() override
    {
        db::translateLmdbErrors([&] { native_.renew(); });
    }
    void reset() noexcept override { native_.reset(); }

    lmdb::txn &native() noexcept { return native_; }
    const lmdb::txn &native() const noexcept { return native_; }
    auto handle() const noexcept { return native_.handle(); }
    const void *env() const noexcept { return native_.env(); }

private:
    lmdb::txn native_;
};

class LmdbCursorImpl final : public db::detail::CursorImpl
{
public:
    explicit LmdbCursorImpl(lmdb::cursor native)
      : native_(std::move(native))
    {
    }

    bool get(std::string_view &key, std::string_view &value, db::CursorOp op) override
    {
        return db::translateLmdbErrors(
          [&] { return native_.get(key, value, db::toLmdbCursorOp(op)); });
    }

    bool get(std::string_view &key, db::CursorOp op) override
    {
        return db::translateLmdbErrors([&] { return native_.get(key, db::toLmdbCursorOp(op)); });
    }

    bool put(std::string_view key, std::string_view value, db::PutFlags flags) override
    {
        return db::translateLmdbErrors(
          [&] { return native_.put(key, value, db::toLmdbPutFlags(flags)); });
    }

    bool del(unsigned flags) override
    {
        db::translateLmdbErrors([&] { native_.del(flags); });
        return true;
    }

    void close() override
    {
        db::translateLmdbErrors([&] { native_.close(); });
    }

private:
    lmdb::cursor native_;
};

db::Error
backendMismatchError(const char *object)
{
    return db::Error(std::string("Database backend mismatch for ") + object,
                     db::ErrorKind::Invalid);
}

LmdbTxnImpl &
requireLmdbTxn(db::detail::TxnImpl &txn)
{
    auto *impl = dynamic_cast<LmdbTxnImpl *>(&txn);
    if (!impl)
        throw backendMismatchError("transaction");
    return *impl;
}

const LmdbTxnImpl *
maybeLmdbTxn(const db::detail::TxnImpl *txn) noexcept
{
    return dynamic_cast<const LmdbTxnImpl *>(txn);
}

class LmdbDbiImpl final : public db::detail::DbiImpl
{
public:
    explicit LmdbDbiImpl(lmdb::dbi native)
      : native_(std::move(native))
    {
    }

    bool get(db::detail::TxnImpl &txn, std::string_view key, std::string_view &value) override
    {
        return db::translateLmdbErrors(
          [&] { return native_.get(requireLmdbTxn(txn).native(), key, value); });
    }

    bool put(db::detail::TxnImpl &txn,
             std::string_view key,
             std::string_view value,
             db::PutFlags flags) override
    {
        return db::translateLmdbErrors([&] {
            return native_.put(requireLmdbTxn(txn).native(), key, value, db::toLmdbPutFlags(flags));
        });
    }

    bool del(db::detail::TxnImpl &txn, std::string_view key) override
    {
        return db::translateLmdbErrors(
          [&] { return native_.del(requireLmdbTxn(txn).native(), key); });
    }

    bool del(db::detail::TxnImpl &txn, std::string_view key, std::string_view value) override
    {
        return db::translateLmdbErrors(
          [&] { return native_.del(requireLmdbTxn(txn).native(), key, value); });
    }

    bool drop(db::detail::TxnImpl &txn, bool del) override
    {
        db::translateLmdbErrors([&] { native_.drop(requireLmdbTxn(txn).native(), del); });
        return true;
    }

    std::size_t size(db::detail::TxnImpl &txn) override
    {
        return db::translateLmdbErrors([&] { return native_.size(requireLmdbTxn(txn).native()); });
    }

    std::unique_ptr<db::detail::CursorImpl> openCursor(db::detail::TxnImpl &txn) override
    {
        return db::translateLmdbErrors([&] {
            return std::make_unique<LmdbCursorImpl>(
              lmdb::cursor::open(requireLmdbTxn(txn).native(), native_));
        });
    }

    lmdb::dbi &native() noexcept { return native_; }
    const lmdb::dbi &native() const noexcept { return native_; }

private:
    lmdb::dbi native_;
};

LmdbDbiImpl &
requireLmdbDbi(db::detail::DbiImpl &dbi)
{
    auto *impl = dynamic_cast<LmdbDbiImpl *>(&dbi);
    if (!impl)
        throw backendMismatchError("database handle");
    return *impl;
}

} // namespace

namespace db {

struct LmdbBackend::Impl
{
    lmdb::env env = nullptr;
};

LmdbBackend::LmdbBackend()
  : impl_(std::make_unique<Impl>())
{
}

LmdbBackend::~LmdbBackend() = default;

bool
LmdbBackend::isOpen() const noexcept
{
    return impl_ && impl_->env.handle() != nullptr;
}

void
LmdbBackend::open(std::string_view directory, const BackendOptions &options)
{
    if (isOpen())
        close();

    auto flags = 0;
    if (options.durability == Durability::Relaxed) {
        flags |= MDB_NOMETASYNC;
        flags |= MDB_NOSYNC;
    }

    translateLmdbErrors([&] {
        const std::string directoryPath{directory};
        impl_->env = lmdb::env::create();
        impl_->env.set_mapsize(options.mapSizeBytes);
        impl_->env.set_max_dbs(options.maxStores);
        impl_->env.open(directoryPath.c_str(), flags);
    });
}

void
LmdbBackend::close() noexcept
{
    if (isOpen())
        impl_->env.close();
}

bool
LmdbBackend::ownsTxn(const Txn &txn) const noexcept
{
    if (!isOpen())
        return false;

    const auto *lmdbTxn = maybeLmdbTxn(detail::txnImpl(txn));
    return lmdbTxn && lmdbTxn->env() == impl_->env.handle();
}

Txn
LmdbBackend::beginTxn(Txn *parent, TxnFlags flags)
{
    auto parentHandle = static_cast<MDB_txn *>(nullptr);
    if (parent) {
        if (!detail::txnImpl(*parent))
            throw Error("Invalid parent transaction", ErrorKind::Invalid);
        parentHandle = requireLmdbTxn(*detail::txnImpl(*parent)).handle();
    }

    return translateLmdbErrors([&] {
        return Txn{std::make_shared<LmdbTxnImpl>(
          lmdb::txn::begin(impl_->env, parentHandle, toLmdbTxnFlags(flags)))};
    });
}

Store
LmdbBackend::openStore(Txn &txn, std::string_view name, const StoreOpenOptions &options)
{
    if (!detail::txnImpl(txn))
        throw Error("Invalid transaction", ErrorKind::Invalid);
    if (name.empty())
        throw Error("Database name must not be empty", ErrorKind::Invalid);

    auto &lmdbTxn    = requireLmdbTxn(*detail::txnImpl(txn));
    const auto flags = options.flags;
    const std::string dbName{name};

    auto dbi = translateLmdbErrors([&] {
        return Store{std::make_shared<LmdbDbiImpl>(
          lmdb::dbi::open(lmdbTxn.native(), dbName.c_str(), toLmdbStoreFlags(flags)))};
    });

    if (options.dupsortComparator.has_value()) {
        if (!hasFlag(flags, StoreFlags::DupSort))
            throw Error("dupsort comparator requires DupSort database flag", ErrorKind::Invalid);

        auto &lmdbDbi = requireLmdbDbi(*detail::dbiImpl(dbi));
        translateLmdbErrors([&] {
            lmdb::dbi_set_dupsort(lmdbTxn.native(),
                                  lmdbDbi.native(),
                                  dupsortComparatorFunc(*options.dupsortComparator));
        });
    }

    return dbi;
}

bool
LmdbBackend::supports(StoreCapability capability) const noexcept
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
LmdbBackend::listStoreNames(Txn &txn)
{
    if (!detail::txnImpl(txn))
        throw Error("Invalid transaction", ErrorKind::Invalid);

    auto &lmdbTxn = requireLmdbTxn(*detail::txnImpl(txn));
    auto rootDb   = Dbi{std::make_shared<LmdbDbiImpl>(
      translateLmdbErrors([&] { return lmdb::dbi::open(lmdbTxn.native()); }))};

    return listUniqueKeys(txn, rootDb);
}

std::optional<std::size_t>
LmdbBackend::mapSizeBytes() const noexcept
{
    if (!isOpen())
        return std::nullopt;

    MDB_envinfo envInfo = {};
    lmdb::env_info(const_cast<lmdb::env &>(impl_->env), &envInfo);
    return envInfo.me_mapsize;
}

} // namespace db
