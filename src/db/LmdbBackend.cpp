// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "db/LmdbBackend.h"

#include <memory>
#include <string_view>

#include <nlohmann/json.hpp>

#include "db/DbTypes.h"
#include "db/LmdbError.h"
#include "db/LmdbFlags.h"

namespace {

std::string_view
stateKeyFromCompositeValue(const MDB_val *value)
{
    auto raw = std::string_view(static_cast<const char *>(value->mv_data), value->mv_size);
    // Allow plain state keys and "state_key\0event_id" composite values.
    return raw.substr(0, raw.rfind('\0'));
}

int
compareStateKey(const MDB_val *a, const MDB_val *b)
{
    return stateKeyFromCompositeValue(a).compare(stateKeyFromCompositeValue(b));
}

std::string
stateKeyFromLegacyJson(const MDB_val *value)
{
    try {
        return nlohmann::json::parse(
                 std::string_view(static_cast<const char *>(value->mv_data), value->mv_size))
          .value("key", "");
    } catch (...) {
        return {};
    }
}

int
compareLegacyStateByKeyJson(const MDB_val *a, const MDB_val *b)
{
    return stateKeyFromLegacyJson(a).compare(stateKeyFromLegacyJson(b));
}

MDB_cmp_func *
dupsortComparator(db::DupsortComparator comparator)
{
    switch (comparator) {
    case db::DupsortComparator::StateKey:
        return compareStateKey;
    case db::DupsortComparator::LegacyStateByKeyJson:
        return compareLegacyStateByKeyJson;
    }

    return compareStateKey;
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
LmdbBackend::open(const QString &directory, const BackendOptions &options)
{
    if (isOpen())
        close();

    auto flags = 0;
    if (options.noMetaSync)
        flags |= MDB_NOMETASYNC;
    if (options.noSync)
        flags |= MDB_NOSYNC;

    translateLmdbErrors([&] {
        impl_->env = lmdb::env::create();
        impl_->env.set_mapsize(options.mapSizeBytes);
        impl_->env.set_max_dbs(options.maxDbs);
        impl_->env.open(directory.toStdString().c_str(), flags);
    });
}

void
LmdbBackend::close() noexcept
{
    if (isOpen())
        impl_->env.close();
}

bool
LmdbBackend::isMapFullError(const std::exception &e) const noexcept
{
    if (auto *dbError = dynamic_cast<const db::Error *>(&e))
        return dbError->kind() == ErrorKind::MapFull;

    auto *lmdbError = dynamic_cast<const lmdb::error *>(&e);
    if (!lmdbError)
        return false;

    return errorKindFromLmdbCode(lmdbError->code()) == ErrorKind::MapFull;
}

bool
LmdbBackend::ownsTxn(const Txn &txn) const noexcept
{
    return isOpen() && txn.env() == impl_->env.handle();
}

Txn
LmdbBackend::beginTxn(Txn *parent, TxnFlags flags)
{
    return translateLmdbErrors([&] {
        return Txn::fromNative(
          lmdb::txn::begin(impl_->env, parent ? parent->handle() : nullptr, toLmdbTxnFlags(flags)));
    });
}

Dbi
LmdbBackend::openDbi(Txn &txn, const char *name, DbiFlags flags)
{
    return translateLmdbErrors([&] {
        if (name)
            return Dbi::fromNative(lmdb::dbi::open(txn.native(), name, toLmdbDbiFlags(flags)));
        return Dbi::fromNative(lmdb::dbi::open(txn.native()));
    });
}

void
LmdbBackend::setDbiDupsort(Txn &txn, Dbi dbi, DupsortComparator comparator)
{
    translateLmdbErrors(
      [&] { lmdb::dbi_set_dupsort(txn.native(), dbi.native(), dupsortComparator(comparator)); });
}

void
LmdbBackend::closeDbi(Dbi dbi) noexcept
{
    if (isOpen())
        lmdb::dbi_close(impl_->env, dbi.native());
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

std::unique_ptr<Backend>
createDefaultBackend()
{
    return std::make_unique<LmdbBackend>();
}

} // namespace db
