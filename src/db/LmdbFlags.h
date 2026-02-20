// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "db/Flags.h"
#include "db/LmdbHeaders.h"

namespace db {

inline unsigned
toLmdbTxnFlags(TxnFlags flags) noexcept
{
    unsigned native = 0;

    if (hasFlag(flags, TxnFlags::ReadOnly))
        native |= MDB_RDONLY;

    return native;
}

inline unsigned
toLmdbDbiFlags(DbiFlags flags) noexcept
{
    unsigned native = 0;

    if (hasFlag(flags, DbiFlags::Create))
        native |= MDB_CREATE;
    if (hasFlag(flags, DbiFlags::IntegerKey))
        native |= MDB_INTEGERKEY;
    if (hasFlag(flags, DbiFlags::DupSort))
        native |= MDB_DUPSORT;

    return native;
}

inline unsigned
toLmdbPutFlags(PutFlags flags) noexcept
{
    unsigned native = 0;

    if (hasFlag(flags, PutFlags::Append))
        native |= MDB_APPEND;
    if (hasFlag(flags, PutFlags::AppendDup))
        native |= MDB_APPENDDUP;

    return native;
}

} // namespace db
