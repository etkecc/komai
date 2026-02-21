// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <span>
#include <string>
#include <string_view>

#include "db/Catalog.h"
#include "db/Backend.h"

namespace db::maintenance {

using Database    = db::Database;
using Transaction = db::Transaction;
using Store       = db::Store;

std::span<const catalog::RoomDb>
roomDbsForFullResync() noexcept;

bool
tryDropNamedStore(Database &database, Transaction &txn, std::string_view dbName, std::string *error = nullptr) noexcept;

void
migrateLegacyOlmShardsV1ToV2(Database &database, Transaction &txn);

bool
migrateLegacyMegolmSessionIndexes(Database &database,
                                 Transaction &txn,
                                 std::string *error = nullptr) noexcept;

bool
migrateLegacyStateByKeyToStatesKey(Database &database,
                                  Transaction &txn,
                                  std::string_view roomId,
                                  std::string *error = nullptr) noexcept;

bool
migrateLegacyOlmShardsV2ToUnified(Database &database, Transaction &txn, Store &olmSessions);

bool
supportsCompaction(const Database &database) noexcept;

bool
supportsCompaction(const Database *database) noexcept;

void
compact(Database &from, Database &to);

}
