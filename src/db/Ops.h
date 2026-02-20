// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string_view>

#include "db/DbTypes.h"
#include "db/LmdbHeaders.h"

namespace db {

using Cursor       = lmdb::cursor;
using Error        = lmdb::error;
using RuntimeError = lmdb::runtime_error;
using MapFullError = lmdb::map_full_error;
using RawTxn       = MDB_txn;
using RawVal       = MDB_val;
using EnvInfo      = MDB_envinfo;
using CompareFn    = MDB_cmp_func;

inline constexpr unsigned kReadOnlyTxn          = MDB_RDONLY;
inline constexpr unsigned kCreate               = MDB_CREATE;
inline constexpr unsigned kIntegerKey           = MDB_INTEGERKEY;
inline constexpr unsigned kDupSort              = MDB_DUPSORT;
inline constexpr unsigned kAppend               = MDB_APPEND;
inline constexpr unsigned kAppendDup            = MDB_APPENDDUP;
inline constexpr int kInvalid                   = MDB_INVALID;
inline constexpr int kVersionMismatch           = MDB_VERSION_MISMATCH;
inline constexpr int kMapFull                   = MDB_MAP_FULL;
inline constexpr int kDbsFull                   = MDB_DBS_FULL;
inline constexpr unsigned kCopyCompact          = MDB_CP_COMPACT;
inline constexpr unsigned kMapAsync             = MDB_MAPASYNC;
inline constexpr unsigned kWriteMap             = MDB_WRITEMAP;
inline constexpr MDB_cursor_op kCursorFirst     = MDB_FIRST;
inline constexpr MDB_cursor_op kCursorFirstDup  = MDB_FIRST_DUP;
inline constexpr MDB_cursor_op kCursorGetBoth   = MDB_GET_BOTH;
inline constexpr MDB_cursor_op kCursorLast      = MDB_LAST;
inline constexpr MDB_cursor_op kCursorNext      = MDB_NEXT;
inline constexpr MDB_cursor_op kCursorNextDup   = MDB_NEXT_DUP;
inline constexpr MDB_cursor_op kCursorNextNoDup = MDB_NEXT_NODUP;
inline constexpr MDB_cursor_op kCursorPrev      = MDB_PREV;
inline constexpr MDB_cursor_op kCursorSet       = MDB_SET;
inline constexpr MDB_cursor_op kCursorSetRange  = MDB_SET_RANGE;

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

inline Cursor
openCursor(Txn &txn, Dbi dbi)
{
    return Cursor::open(txn, dbi);
}

inline void
cursorDel(Cursor &cursor)
{
    lmdb::cursor_del(cursor);
}

inline void
dbiDrop(Txn &txn, Dbi dbi, bool del)
{
    lmdb::dbi_drop(txn, dbi, del);
}

inline void
dbiSetDupsort(Txn &txn, Dbi dbi, CompareFn *cmp)
{
    lmdb::dbi_set_dupsort(txn, dbi, cmp);
}

inline void
dbiClose(Env &env, Dbi dbi)
{
    lmdb::dbi_close(env, dbi);
}

inline void
envInfo(Env &env, EnvInfo *envinfo)
{
    lmdb::env_info(env, envinfo);
}

inline void
envCopy(Env &env, const char *path, unsigned flags)
{
    lmdb::env_copy(env, path, flags);
}

} // namespace db
