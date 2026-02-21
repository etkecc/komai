// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "db/Catalog.h"

namespace db {

class Txn;
class Dbi;

bool
getSyncStateValue(Txn &txn, Dbi &syncStateDb, catalog::SyncStateKey key, std::string_view &value);

std::optional<std::string>
getSyncStateValue(Txn &txn, Dbi &syncStateDb, catalog::SyncStateKey key);

void
putSyncStateValue(Txn &txn, Dbi &syncStateDb, catalog::SyncStateKey key, std::string_view value);

bool
removeSyncStateValue(Txn &txn, Dbi &syncStateDb, catalog::SyncStateKey key);

bool
getSyncStateSecretValue(Txn &txn,
                        Dbi &syncStateDb,
                        std::string_view secretName,
                        std::string_view &value);

std::optional<std::string>
getSyncStateSecretValue(Txn &txn, Dbi &syncStateDb, std::string_view secretName);

void
putSyncStateSecretValue(Txn &txn,
                        Dbi &syncStateDb,
                        std::string_view secretName,
                        std::string_view value);

bool
removeSyncStateSecretValue(Txn &txn, Dbi &syncStateDb, std::string_view secretName);

} // namespace db
