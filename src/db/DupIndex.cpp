// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "db/DupIndex.h"

#include "db/DbTypes.h"

namespace db {

std::vector<std::string>
listDupValues(Txn &txn, Dbi &db, std::string_view key)
{
    std::vector<std::string> values;
    auto cursor = Cursor::open(txn, db);

    std::string_view cursorKey = key;
    std::string_view value;
    bool first = true;

    if (!cursor.get(cursorKey, value, CursorOp::Set))
        return values;

    while (cursor.get(cursorKey, value, first ? CursorOp::FirstDup : CursorOp::NextDup)) {
        first = false;
        if (cursorKey != key)
            break;

        values.emplace_back(value);
    }

    return values;
}

} // namespace db
