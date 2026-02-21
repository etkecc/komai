// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace db {

class Txn;
class Dbi;

void
forEachDupValue(Txn &txn,
                Dbi &db,
                std::string_view key,
                const std::function<bool(std::string_view value)> &visitor);

std::vector<std::string>
listDupValues(Txn &txn, Dbi &db, std::string_view key);

std::size_t
putDupValueForKeys(Txn &txn,
                   Dbi &db,
                   std::span<const std::string_view> keys,
                   std::string_view value);

std::size_t
replaceDupValueForKeys(Txn &txn,
                       Dbi &db,
                       std::span<const std::string_view> keys,
                       std::string_view oldValue,
                       std::string_view newValue);

} // namespace db
