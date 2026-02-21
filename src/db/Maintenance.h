// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <span>
#include <string>
#include <string_view>

#include "db/Catalog.h"
#include "db/StorageApi.h"

namespace db::maintenance {

using Database    = db::storage::Database;
using Transaction = db::storage::Transaction;
using Store       = db::storage::Store;

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

}
