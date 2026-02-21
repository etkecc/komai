// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace db {

class Txn;
class Dbi;

using Transaction = Txn;
using Store       = Dbi;

enum class ScanDirection
{
    Forward,
    Backward,
};

std::vector<std::string>
listKeys(Transaction &txn, Store &db);

std::vector<std::string>
listUniqueKeys(Transaction &txn, Store &db);

std::vector<std::pair<std::string, std::string>>
listEntries(Transaction &txn, Store &db);

std::vector<std::pair<std::string, std::string>>
listEntries(Transaction &txn, Store &db, std::size_t startIndex, std::size_t limit);

void
forEachEntry(Transaction &txn,
             Store &db,
             const std::function<bool(std::string_view key, std::string_view value)> &visitor);

void
forEachUniqueKey(Transaction &txn, Store &db, const std::function<bool(std::string_view key)> &visitor);

void
forEachEntry(Transaction &txn,
             Store &db,
             std::size_t startIndex,
             std::size_t limit,
             const std::function<bool(std::string_view key, std::string_view value)> &visitor);

std::optional<std::pair<std::string, std::string>>
firstEntry(Transaction &txn, Store &db);

std::optional<std::pair<std::string, std::string>>
lastEntry(Transaction &txn, Store &db);

std::size_t
eraseEntriesIf(Transaction &txn,
               Store &db,
               const std::function<bool(std::string_view key, std::string_view value)> &predicate);

std::size_t
eraseEntriesIf(Transaction &txn,
               Store &db,
               std::size_t startIndex,
               std::size_t limit,
               const std::function<bool(std::string_view key, std::string_view value)> &predicate);

void
forEachEntryFromKey(
  Transaction &txn,
  Store &db,
  std::string_view startKey,
  ScanDirection direction,
  const std::function<bool(std::string_view key, std::string_view value)> &visitor);

void
forEachEntryWithPrefix(
  Transaction &txn,
  Store &db,
  std::string_view prefix,
  const std::function<bool(std::string_view key, std::string_view value)> &visitor);

} // namespace db
