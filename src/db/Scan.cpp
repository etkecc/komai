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

std::vector<std::pair<std::string, std::string>>
listEntries(Txn &txn, Dbi &db)
{
    std::vector<std::pair<std::string, std::string>> entries;
    auto cursor = Cursor::open(txn, db);

    std::string_view key;
    std::string_view value;
    while (cursor.get(key, value, CursorOp::Next))
        entries.emplace_back(std::string(key), std::string(value));

    return entries;
}

std::vector<std::pair<std::string, std::string>>
listEntries(Txn &txn, Dbi &db, std::size_t startIndex, std::size_t limit)
{
    std::vector<std::pair<std::string, std::string>> entries;
    if (limit == 0)
        return entries;

    auto cursor = Cursor::open(txn, db);

    std::size_t currentIndex = 0;
    std::size_t remaining    = limit;

    std::string_view key;
    std::string_view value;
    while (cursor.get(key, value, CursorOp::Next)) {
        if (currentIndex < startIndex) {
            currentIndex += 1;
            continue;
        }

        if (remaining == 0)
            break;

        entries.emplace_back(std::string(key), std::string(value));
        currentIndex += 1;
        remaining -= 1;
    }

    return entries;
}

void
forEachEntry(Txn &txn,
             Dbi &db,
             const std::function<bool(std::string_view key, std::string_view value)> &visitor)
{
    auto cursor = Cursor::open(txn, db);

    std::string_view key;
    std::string_view value;
    while (cursor.get(key, value, CursorOp::Next)) {
        if (!visitor(key, value))
            break;
    }
}

void
forEachEntry(Txn &txn,
             Dbi &db,
             std::size_t startIndex,
             std::size_t limit,
             const std::function<bool(std::string_view key, std::string_view value)> &visitor)
{
    if (limit == 0)
        return;

    auto cursor = Cursor::open(txn, db);

    std::size_t currentIndex = 0;
    std::size_t remaining    = limit;

    std::string_view key;
    std::string_view value;
    while (cursor.get(key, value, CursorOp::Next)) {
        if (currentIndex < startIndex) {
            currentIndex += 1;
            continue;
        }

        if (remaining == 0)
            break;

        if (!visitor(key, value))
            break;

        currentIndex += 1;
        remaining -= 1;
    }
}

std::optional<std::pair<std::string, std::string>>
firstEntry(Txn &txn, Dbi &db)
{
    auto cursor = Cursor::open(txn, db);

    std::string_view key;
    std::string_view value;
    if (!cursor.get(key, value, CursorOp::First))
        return std::nullopt;

    return std::pair(std::string(key), std::string(value));
}

std::optional<std::pair<std::string, std::string>>
lastEntry(Txn &txn, Dbi &db)
{
    auto cursor = Cursor::open(txn, db);

    std::string_view key;
    std::string_view value;
    if (!cursor.get(key, value, CursorOp::Last))
        return std::nullopt;

    return std::pair(std::string(key), std::string(value));
}

void
forEachEntryFromKey(
  Txn &txn,
  Dbi &db,
  std::string_view startKey,
  ScanDirection direction,
  const std::function<bool(std::string_view key, std::string_view value)> &visitor)
{
    auto cursor = Cursor::open(txn, db);

    std::string_view key = startKey;
    std::string_view value;
    if (!cursor.get(key, value, CursorOp::Set))
        return;

    const auto nextOp = direction == ScanDirection::Forward ? CursorOp::Next : CursorOp::Prev;
    do {
        if (!visitor(key, value))
            break;
    } while (cursor.get(key, value, nextOp));
}

void
forEachEntryWithPrefix(
  Txn &txn,
  Dbi &db,
  std::string_view prefix,
  const std::function<bool(std::string_view key, std::string_view value)> &visitor)
{
    auto cursor = Cursor::open(txn, db);

    std::string_view key = prefix;
    std::string_view value;
    if (!cursor.get(key, value, CursorOp::SetRange))
        return;

    do {
        if (!key.starts_with(prefix))
            break;
        if (!visitor(key, value))
            break;
    } while (cursor.get(key, value, CursorOp::Next));
}

} // namespace db
