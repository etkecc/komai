// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

namespace db {

inline constexpr unsigned kReadOnlyTxn = 1u << 0;
inline constexpr unsigned kCreate      = 1u << 1;
inline constexpr unsigned kIntegerKey  = 1u << 2;
inline constexpr unsigned kDupSort     = 1u << 3;
inline constexpr unsigned kAppend      = 1u << 4;
inline constexpr unsigned kAppendDup   = 1u << 5;
inline constexpr unsigned kMapAsync    = 1u << 6;
inline constexpr unsigned kWriteMap    = 1u << 7;

} // namespace db
