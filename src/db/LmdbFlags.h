// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "db/CursorOp.h"
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
toLmdbStoreFlags(StoreFlags flags) noexcept
{
    unsigned native = 0;

    if (hasFlag(flags, StoreFlags::Create))
        native |= MDB_CREATE;
    if (hasFlag(flags, StoreFlags::IntegerKey))
        native |= MDB_INTEGERKEY;
    if (hasFlag(flags, StoreFlags::DupSort))
        native |= MDB_DUPSORT;

    return native;
}

inline unsigned
toLmdbDbiFlags(StoreFlags flags) noexcept
{
    return toLmdbStoreFlags(flags);
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

inline MDB_cursor_op
toLmdbCursorOp(CursorOp op) noexcept
{
    switch (op) {
    case CursorOp::First:
        return MDB_FIRST;
    case CursorOp::FirstDup:
        return MDB_FIRST_DUP;
    case CursorOp::GetBoth:
        return MDB_GET_BOTH;
    case CursorOp::Last:
        return MDB_LAST;
    case CursorOp::Next:
        return MDB_NEXT;
    case CursorOp::NextDup:
        return MDB_NEXT_DUP;
    case CursorOp::NextNoDup:
        return MDB_NEXT_NODUP;
    case CursorOp::Prev:
        return MDB_PREV;
    case CursorOp::Set:
        return MDB_SET;
    case CursorOp::SetRange:
        return MDB_SET_RANGE;
    }

    return MDB_NEXT;
}

} // namespace db
