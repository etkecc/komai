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

std::span<const catalog::RoomDb>
roomDbsForFullResync() noexcept;

bool
tryDropNamedDbi(Backend &backend,
                Txn &txn,
                std::string_view dbName,
                std::string *error = nullptr) noexcept;

bool
migrateLegacyStateByKeyToStatesKey(Backend &backend,
                                   Txn &txn,
                                   std::string_view roomId,
                                   std::string *error = nullptr) noexcept;

bool
migrateLegacyMegolmSessionIndexes(Backend &backend,
                                  Txn &txn,
                                  std::string *error = nullptr) noexcept;

void
migrateLegacyOlmShardsV1ToV2(Backend &backend, Txn &txn);

bool
migrateLegacyOlmShardsV2ToUnified(Backend &backend, Txn &txn, Dbi &olmSessions);

} // namespace db
