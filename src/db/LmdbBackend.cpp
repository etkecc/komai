// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "db/LmdbBackend.h"

#include <memory>

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

bool
LmdbBackend::isMapFullError(const std::exception &e) const noexcept
{
    return dynamic_cast<const lmdb::map_full_error *>(&e) != nullptr;
}

Txn
LmdbBackend::beginTxn(Txn *parent, unsigned flags)
{
    return Txn::begin(env_, parent ? parent->handle() : nullptr, flags);
}

void
LmdbBackend::closeDbi(Dbi dbi) noexcept
{
    if (isOpen())
        lmdb::dbi_close(env_, dbi);
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
