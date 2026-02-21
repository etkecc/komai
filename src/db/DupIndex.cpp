// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "db/DupIndex.h"

#include "db/DbTypes.h"
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

std::size_t
putDupValueForKeys(Txn &txn,
                   Dbi &db,
                   std::span<const std::string_view> keys,
                   std::string_view value)
{
    if (value.empty())
        return 0;

    std::size_t written = 0;
    for (const auto key : keys) {
        if (key.empty())
            continue;
        db.put(txn, key, value);
        written += 1;
    }

    return written;
}

std::size_t
replaceDupValueForKeys(Txn &txn,
                       Dbi &db,
                       std::span<const std::string_view> keys,
                       std::string_view oldValue,
                       std::string_view newValue)
{
    if (oldValue.empty() || newValue.empty() || oldValue == newValue)
        return 0;

    std::size_t rewritten = 0;
    for (const auto key : keys) {
        if (key.empty())
            continue;
        db.del(txn, key, oldValue);
        db.put(txn, key, newValue);
        rewritten += 1;
    }

    return rewritten;
}

} // namespace db
