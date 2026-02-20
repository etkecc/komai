// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "db/Flags.h"
#include "db/LmdbHeaders.h"

namespace db {

inline unsigned
toLmdbTxnFlags(unsigned flags) noexcept
{
    unsigned native = 0;

    if (flags & kReadOnlyTxn)
        native |= MDB_RDONLY;

    return native;
}

inline unsigned
toLmdbDbiFlags(unsigned flags) noexcept
{
    unsigned native = 0;

    if (flags & kCreate)
        native |= MDB_CREATE;
    if (flags & kIntegerKey)
        native |= MDB_INTEGERKEY;
    if (flags & kDupSort)
        native |= MDB_DUPSORT;

    return native;
}

inline unsigned
toLmdbPutFlags(unsigned flags) noexcept
{
    unsigned native = 0;

    if (flags & kAppend)
        native |= MDB_APPEND;
    if (flags & kAppendDup)
        native |= MDB_APPENDDUP;

    return native;
}

} // namespace db
