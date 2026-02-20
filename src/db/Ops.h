// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string_view>

#include "db/DbTypes.h"
#include "db/LmdbHeaders.h"

namespace db {

using Error = lmdb::error;

inline constexpr unsigned kReadOnlyTxn     = MDB_RDONLY;
inline constexpr unsigned kCreate          = MDB_CREATE;
inline constexpr unsigned kIntegerKey      = MDB_INTEGERKEY;
inline constexpr unsigned kDupSort         = MDB_DUPSORT;
inline constexpr unsigned kAppend          = MDB_APPEND;
inline constexpr unsigned kAppendDup       = MDB_APPENDDUP;
inline constexpr unsigned kMapAsync        = MDB_MAPASYNC;
inline constexpr unsigned kWriteMap        = MDB_WRITEMAP;
inline constexpr CursorOp kCursorFirst     = CursorOp::First;
inline constexpr CursorOp kCursorFirstDup  = CursorOp::FirstDup;
inline constexpr CursorOp kCursorGetBoth   = CursorOp::GetBoth;
inline constexpr CursorOp kCursorLast      = CursorOp::Last;
inline constexpr CursorOp kCursorNext      = CursorOp::Next;
inline constexpr CursorOp kCursorNextDup   = CursorOp::NextDup;
inline constexpr CursorOp kCursorNextNoDup = CursorOp::NextNoDup;
inline constexpr CursorOp kCursorPrev      = CursorOp::Prev;
inline constexpr CursorOp kCursorSet       = CursorOp::Set;
inline constexpr CursorOp kCursorSetRange  = CursorOp::SetRange;

template<typename T>
inline auto
toSv(const T &value)
{
    return lmdb::to_sv(value);
}

template<typename T>
inline T
fromSv(std::string_view value)
{
    return lmdb::from_sv<T>(value);
}

} // namespace db
