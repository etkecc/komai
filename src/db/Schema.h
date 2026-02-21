// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <span>
#include <string>
#include <string_view>

#include "db/Catalog.h"
#include "db/StorageApi.h"

namespace db {

std::span<const catalog::RoomDb>
roomDbsForFullResync() noexcept;

bool
tryDropNamedStore(storage::Database &database,
                  storage::Transaction &txn,
                  std::string_view dbName,
                  std::string *error = nullptr) noexcept;

bool
migrateLegacyStateByKeyToStatesKey(storage::Database &database,
                                   storage::Transaction &txn,
                                   std::string_view roomId,
                                   std::string *error = nullptr) noexcept;

bool
migrateLegacyMegolmSessionIndexes(storage::Database &database,
                                  storage::Transaction &txn,
                                  std::string *error = nullptr) noexcept;

void
migrateLegacyOlmShardsV1ToV2(storage::Database &database, storage::Transaction &txn);

bool
migrateLegacyOlmShardsV2ToUnified(storage::Database &database,
                                  storage::Transaction &txn,
                                  storage::Store &olmSessions);

} // namespace db
