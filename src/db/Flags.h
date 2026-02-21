// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <type_traits>

namespace db {

enum class TxnFlags : unsigned
{
    None     = 0,
    ReadOnly = 1u << 0,
};

enum class StoreFlags : unsigned
{
    None       = 0,
    Create     = 1u << 0,
    IntegerKey = 1u << 1,
    DupSort    = 1u << 2,
};

using DbiFlags = StoreFlags;

enum class PutFlags : unsigned
{
    None      = 0,
    Append    = 1u << 0,
    AppendDup = 1u << 1,
};

template<typename Flag>
constexpr auto
toUnderlying(Flag flag) noexcept
{
    static_assert(std::is_enum_v<Flag>);
    return static_cast<std::underlying_type_t<Flag>>(flag);
}

template<typename Flag>
constexpr Flag
operator|(Flag lhs, Flag rhs) noexcept
{
    static_assert(std::is_enum_v<Flag>);
    return static_cast<Flag>(toUnderlying(lhs) | toUnderlying(rhs));
}

template<typename Flag>
constexpr Flag &
operator|=(Flag &lhs, Flag rhs) noexcept
{
    lhs = lhs | rhs;
    return lhs;
}

template<typename Flag>
constexpr bool
hasFlag(Flag value, Flag flag) noexcept
{
    static_assert(std::is_enum_v<Flag>);
    return (toUnderlying(value) & toUnderlying(flag)) != 0;
}

} // namespace db
