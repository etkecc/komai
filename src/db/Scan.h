// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>
#include <utility>
#include <vector>

namespace db {

class Txn;
class Dbi;

std::vector<std::string>
listKeys(Txn &txn, Dbi &db);

std::vector<std::pair<std::string, std::string>>
listEntries(Txn &txn, Dbi &db);

} // namespace db
