// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>

#include "db/CursorOp.h"
#include "db/Error.h"
#include "db/Flags.h"

namespace db {

inline constexpr TxnFlags kReadOnlyTxn = TxnFlags::ReadOnly;
inline constexpr DbiFlags kCreate      = DbiFlags::Create;
inline constexpr DbiFlags kIntegerKey  = DbiFlags::IntegerKey;
inline constexpr DbiFlags kDupSort     = DbiFlags::DupSort;
inline constexpr PutFlags kAppend      = PutFlags::Append;
inline constexpr PutFlags kAppendDup   = PutFlags::AppendDup;
inline constexpr EnvFlags kMapAsync    = EnvFlags::MapAsync;
inline constexpr EnvFlags kWriteMap    = EnvFlags::WriteMap;

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
    requires(std::is_integral_v<T> || std::is_enum_v<T>)
inline std::string_view
toSv(const T &value)
{
    return std::string_view(reinterpret_cast<const char *>(&value), sizeof(T));
}

inline std::string_view
toSv(std::string_view value)
{
    return value;
}

inline std::string_view
toSv(const std::string &value)
{
    return value;
}

inline std::string_view
toSv(const char *value)
{
    return value ? std::string_view(value) : std::string_view{};
}

template<typename T>
    requires(std::is_integral_v<T> || std::is_enum_v<T>)
inline T
fromSv(std::string_view value)
{
    if (value.size() != sizeof(T))
        throw Error("Invalid scalar size in fromSv", ErrorKind::Invalid);

    T out{};
    std::memcpy(&out, value.data(), sizeof(T));
    return out;
}

} // namespace db
