// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>
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

} // namespace db
