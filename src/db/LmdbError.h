// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <utility>

#include "db/Error.h"
#include "db/LmdbHeaders.h"

namespace db {

inline ErrorKind
errorKindFromLmdbCode(int code) noexcept
{
    switch (code) {
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

inline Error
errorFromLmdb(const lmdb::error &error)
{
    return Error(error.what(), errorKindFromLmdbCode(error.code()));
}

template<typename Fn>
decltype(auto)
translateLmdbErrors(Fn &&fn)
{
    try {
        return std::forward<Fn>(fn)();
    } catch (const lmdb::error &error) {
        throw errorFromLmdb(error);
    }
}

} // namespace db
