// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "db/DupIndex.h"

#include "db/Scan.h"

namespace db {

void
forEachDupValue(Txn &txn,
                Dbi &db,
                std::string_view key,
                const std::function<bool(std::string_view value)> &visitor)
{
    forEachEntryFromKey(txn,
                        db,
                        key,
                        ScanDirection::Forward,
                        [key, &visitor](std::string_view scannedKey, std::string_view value) {
                            if (scannedKey != key)
                                return false;
                            return visitor(value);
                        });
}

std::vector<std::string>
listDupValues(Txn &txn, Dbi &db, std::string_view key)
{
    std::vector<std::string> values;
    forEachDupValue(txn, db, key, [&values](std::string_view value) {
        values.emplace_back(value);
        return true;
    });

    return values;
}

} // namespace db
