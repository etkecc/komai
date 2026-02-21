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

bool
getOlmSessionValue(Txn &txn,
                   Dbi &olmSessionsDb,
                   std::string_view curve25519,
                   std::string_view sessionId,
                   std::string_view &value);

void
putOlmSessionValue(Txn &txn,
                   Dbi &olmSessionsDb,
                   std::string_view curve25519,
                   std::string_view sessionId,
                   std::string_view value);

std::size_t
forEachOlmSessionForCurve(
  Txn &txn,
  Dbi &olmSessionsDb,
  std::string_view curve25519,
  const std::function<bool(std::string_view sessionId, std::string_view value)> &callback);

std::vector<std::string>
listOlmSessionIds(Txn &txn, Dbi &olmSessionsDb, std::string_view curve25519);

} // namespace db
