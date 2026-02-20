// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace db {

class Txn;
class Dbi;

std::vector<std::string>
listDupValues(Txn &txn, Dbi &db, std::string_view key);

} // namespace db
