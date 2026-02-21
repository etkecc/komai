// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <span>
#include <string>
#include <string_view>

#include "db/Catalog.h"

namespace db {

class Backend;
class Txn;
class Dbi;

using Transaction = Txn;
using Store = Dbi;

std::span<const catalog::RoomDb>
roomDbsForFullResync() noexcept;

bool
tryDropNamedStore(Backend &backend,
                  Transaction &txn,
                  std::string_view dbName,
                  std::string *error = nullptr) noexcept;

bool
tryDropNamedDbi(Backend &backend, Transaction &txn, std::string_view dbName, std::string *error = nullptr) noexcept;

bool
migrateLegacyStateByKeyToStatesKey(Backend &backend,
                                   Transaction &txn,
                                   std::string_view roomId,
                                   std::string *error = nullptr) noexcept;

bool
migrateLegacyMegolmSessionIndexes(Backend &backend,
                                  Transaction &txn,
                                  std::string *error = nullptr) noexcept;

void
migrateLegacyOlmShardsV1ToV2(Backend &backend, Transaction &txn);

bool
migrateLegacyOlmShardsV2ToUnified(Backend &backend, Transaction &txn, Store &olmSessions);

} // namespace db
