// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "db/LmdbBackend.h"

#include <memory>
#include <string_view>

#include <nlohmann/json.hpp>

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

    env_ = lmdb::env::create();
    env_.set_mapsize(options.mapSizeBytes);
    env_.set_max_dbs(options.maxDbs);
    env_.open(directory.toStdString().c_str(), flags);
}

void
LmdbBackend::close() noexcept
{
    if (isOpen())
        env_.close();
}

ErrorKind
LmdbBackend::classifyError(const std::exception &e) const noexcept
{
    auto *lmdbError = dynamic_cast<const lmdb::error *>(&e);
    if (!lmdbError)
        return ErrorKind::Unknown;

    switch (lmdbError->code()) {
    case MDB_VERSION_MISMATCH:
        return ErrorKind::VersionMismatch;
    case MDB_INVALID:
        return ErrorKind::Invalid;
    case MDB_MAP_FULL:
        return ErrorKind::MapFull;
    case MDB_DBS_FULL:
        return ErrorKind::DbsFull;
    default:
        return ErrorKind::Unknown;
    }
}

Txn
LmdbBackend::beginTxn(Txn *parent, unsigned flags)
{
    return Txn::fromNative(lmdb::txn::begin(env_, parent ? parent->handle() : nullptr, flags));
}

Dbi
LmdbBackend::openDbi(Txn &txn, const char *name, unsigned flags)
{
    if (name)
        return Dbi::fromNative(lmdb::dbi::open(txn.native(), name, flags));
    return Dbi::fromNative(lmdb::dbi::open(txn.native()));
}

void
LmdbBackend::setDbiDupsort(Txn &txn, Dbi dbi, DupsortComparator comparator)
{
    lmdb::dbi_set_dupsort(txn.native(), dbi.native(), dupsortComparator(comparator));
}

void
LmdbBackend::closeDbi(Dbi dbi) noexcept
{
    if (isOpen())
        lmdb::dbi_close(env_, dbi.native());
}

std::optional<std::size_t>
LmdbBackend::mapSizeBytes() const noexcept
{
    if (!isOpen())
        return std::nullopt;

    MDB_envinfo envInfo = {};
    lmdb::env_info(const_cast<lmdb::env &>(env_), &envInfo);
    return envInfo.me_mapsize;
}

std::unique_ptr<Backend>
createDefaultBackend()
{
    return std::make_unique<LmdbBackend>();
}

} // namespace db
