// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstring>
#include <string>
#include <string_view>
#include <type_traits>

#include "db/Error.h"

namespace db {

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
