// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "db/Compaction.h"

#include "db/DbTypes.h"
#include "db/Open.h"

namespace db {

void
compact(Backend &from, Backend &to)
{
    auto fromTxn = from.beginTxn(nullptr, TxnFlags::ReadOnly);
    auto toTxn   = to.beginTxn();

    const auto dbNames = from.listDbiNames(fromTxn);
    for (const auto &dbName : dbNames) {
        auto fromDb = openNamedDbi(from, fromTxn, dbName, false);
        auto toDb   = openNamedDbi(to, toTxn, dbName, true);

        auto fromCursor = Cursor::open(fromTxn, fromDb);
        auto toCursor   = Cursor::open(toTxn, toDb);

        std::string_view key, val;
        while (fromCursor.get(key, val, CursorOp::Next)) {
            toCursor.put(key, val, PutFlags::AppendDup);
        }
    }

    toTxn.commit();
}

} // namespace db
