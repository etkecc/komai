// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "db/Scan.h"

#include <limits>

#include "db/StorageApi.h"

namespace db {

std::vector<std::string>
listKeys(Transaction &txn, Store &db)
{
    std::vector<std::string> keys;
    auto cursor = storage::Cursor::open(txn, db);

    std::string key;
    std::string value;
    while (cursor.moveNext(key, value))
        keys.emplace_back(key);

    return keys;
}

std::vector<std::string>
listUniqueKeys(Transaction &txn, Store &db)
{
    std::vector<std::string> keys;
    auto cursor = storage::Cursor::open(txn, db);

    std::string key;
    std::string value;
    while (cursor.moveNextNoDup(key, value))
        keys.emplace_back(key);

    return keys;
}

std::vector<std::pair<std::string, std::string>>
listEntries(Transaction &txn, Store &db)
{
    std::vector<std::pair<std::string, std::string>> entries;
    auto cursor = storage::Cursor::open(txn, db);

    std::string key;
    std::string value;
    while (cursor.moveNext(key, value))
        entries.emplace_back(key, value);

    return entries;
}

std::vector<std::pair<std::string, std::string>>
listEntries(Transaction &txn, Store &db, std::size_t startIndex, std::size_t limit)
{
    std::vector<std::pair<std::string, std::string>> entries;
    if (limit == 0)
        return entries;

    auto cursor = storage::Cursor::open(txn, db);

    std::size_t currentIndex = 0;
    std::size_t remaining    = limit;

    std::string key;
    std::string value;
    while (cursor.moveNext(key, value)) {
        if (currentIndex < startIndex) {
            currentIndex += 1;
            continue;
        }

        if (remaining == 0)
            break;

        entries.emplace_back(std::move(key), std::move(value));
        currentIndex += 1;
        remaining -= 1;
        key.clear();
        value.clear();
    }

    return entries;
}

void
forEachEntry(Transaction &txn,
             Store &db,
             const std::function<bool(std::string_view key, std::string_view value)> &visitor)
{
    auto cursor = storage::Cursor::open(txn, db);

    std::string key;
    std::string value;
    while (cursor.moveNext(key, value)) {
        if (!visitor(key, value))
            break;
        value.clear();
    }
}

void
forEachUniqueKey(Transaction &txn, Store &db, const std::function<bool(std::string_view key)> &visitor)
{
    auto cursor = storage::Cursor::open(txn, db);

    std::string key;
    std::string value;
    while (cursor.moveNextNoDup(key, value)) {
        if (!visitor(key))
            break;
        value.clear();
    }
}

void
forEachEntry(Transaction &txn,
             Store &db,
             std::size_t startIndex,
             std::size_t limit,
             const std::function<bool(std::string_view key, std::string_view value)> &visitor)
{
    if (limit == 0)
        return;

    auto cursor = storage::Cursor::open(txn, db);

    std::size_t currentIndex = 0;
    std::size_t remaining    = limit;

    std::string key;
    std::string value;
    while (cursor.moveNext(key, value)) {
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
firstEntry(Transaction &txn, Store &db)
{
    auto cursor = storage::Cursor::open(txn, db);

    std::string key;
    std::string value;
    if (!cursor.moveFirst(key, value))
        return std::nullopt;

    return std::pair(std::move(key), std::move(value));
}

std::optional<std::pair<std::string, std::string>>
lastEntry(Transaction &txn, Store &db)
{
    auto cursor = storage::Cursor::open(txn, db);

    std::string key;
    std::string value;
    if (!cursor.moveLast(key, value))
        return std::nullopt;

    return std::pair(std::move(key), std::move(value));
}

std::size_t
eraseEntriesIf(Transaction &txn,
               Store &db,
               const std::function<bool(std::string_view key, std::string_view value)> &predicate)
{
    return eraseEntriesIf(txn, db, 0, std::numeric_limits<std::size_t>::max(), predicate);
}

std::size_t
eraseEntriesIf(Transaction &txn,
               Store &db,
               std::size_t startIndex,
               std::size_t limit,
               const std::function<bool(std::string_view key, std::string_view value)> &predicate)
{
    if (limit == 0)
        return 0;

    std::vector<std::pair<std::string, std::string>> entriesToDelete;
    forEachEntry(
      txn,
      db,
      startIndex,
      limit,
      [&entriesToDelete, &predicate](std::string_view key, std::string_view value) {
          if (predicate(key, value))
              entriesToDelete.emplace_back(std::string(key), std::string(value));
          return true;
      });

    for (const auto &[key, value] : entriesToDelete)
        db.del(txn, key, value);

    return entriesToDelete.size();
}

void
forEachEntryFromKey(
  Transaction &txn,
  Store &db,
  std::string_view startKey,
  ScanDirection direction,
  const std::function<bool(std::string_view key, std::string_view value)> &visitor)
{
    auto cursor = storage::Cursor::open(txn, db);

    std::string key;
    std::string value;
    if (!cursor.moveTo(startKey, key, value))
        return;

    const auto moveForward = direction == ScanDirection::Forward;
    while (true) {
        if (!visitor(key, value))
            break;

        if (moveForward) {
            if (!cursor.moveNext(key, value))
                break;
        } else if (!cursor.movePrev(key, value)) {
            break;
        }
    }
}

void
forEachEntryWithPrefix(
  Transaction &txn,
  Store &db,
  std::string_view prefix,
  const std::function<bool(std::string_view key, std::string_view value)> &visitor)
{
    auto cursor = storage::Cursor::open(txn, db);

    std::string key;
    std::string value;
    if (!cursor.moveToRange(prefix, key, value))
        return;

    do {
        if (!key.starts_with(prefix))
            break;
        if (!visitor(key, value))
            break;
    } while (cursor.moveNext(key, value));
}

} // namespace db
