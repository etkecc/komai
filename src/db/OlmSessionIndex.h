// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace db {

class Txn;
class Dbi;

using Transaction = Txn;
using Store       = Dbi;

bool
getOlmSessionValue(Transaction &txn,
                   Store &olmSessionsDb,
                   std::string_view curve25519,
                   std::string_view sessionId,
                   std::string_view &value);

void
putOlmSessionValue(Transaction &txn,
                   Store &olmSessionsDb,
                   std::string_view curve25519,
                   std::string_view sessionId,
                   std::string_view value);

std::size_t
forEachOlmSessionForCurve(
  Transaction &txn,
  Store &olmSessionsDb,
  std::string_view curve25519,
  const std::function<bool(std::string_view sessionId, std::string_view value)> &callback);

std::vector<std::string>
listOlmSessionIds(Transaction &txn, Store &olmSessionsDb, std::string_view curve25519);

} // namespace db
