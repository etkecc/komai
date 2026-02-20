// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "db/Scan.h"

#include "db/DbTypes.h"

namespace db {

std::vector<std::string>
listKeys(Txn &txn, Dbi &db)
{
    std::vector<std::string> keys;
    auto cursor = Cursor::open(txn, db);

    std::string_view key;
    while (cursor.get(key, CursorOp::Next))
        keys.emplace_back(key);

    return keys;
}

} // namespace db
