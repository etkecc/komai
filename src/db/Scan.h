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

enum class ScanDirection
{
    Forward,
    Backward,
};

std::vector<std::string>
listKeys(Txn &txn, Dbi &db);

std::vector<std::pair<std::string, std::string>>
listEntries(Txn &txn, Dbi &db);

std::vector<std::pair<std::string, std::string>>
listEntries(Txn &txn, Dbi &db, std::size_t startIndex, std::size_t limit);

void
forEachEntry(Txn &txn,
             Dbi &db,
             const std::function<bool(std::string_view key, std::string_view value)> &visitor);

void
forEachEntry(Txn &txn,
             Dbi &db,
             std::size_t startIndex,
             std::size_t limit,
             const std::function<bool(std::string_view key, std::string_view value)> &visitor);

std::optional<std::pair<std::string, std::string>>
firstEntry(Txn &txn, Dbi &db);

std::optional<std::pair<std::string, std::string>>
lastEntry(Txn &txn, Dbi &db);

void
forEachEntryFromKey(
  Txn &txn,
  Dbi &db,
  std::string_view startKey,
  ScanDirection direction,
  const std::function<bool(std::string_view key, std::string_view value)> &visitor);

void
forEachEntryWithPrefix(
  Txn &txn,
  Dbi &db,
  std::string_view prefix,
  const std::function<bool(std::string_view key, std::string_view value)> &visitor);

} // namespace db
