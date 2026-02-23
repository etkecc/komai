// SPDX-FileCopyrightText: Nheko Contributors
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

using Transaction = Txn;
using Store       = Dbi;

bool
getSyncStateValue(Transaction &txn,
                  Store &syncStateDb,
                  catalog::SyncStateKey key,
                  std::string_view &value);

std::optional<std::string>
getSyncStateValue(Transaction &txn, Store &syncStateDb, catalog::SyncStateKey key);

void
putSyncStateValue(Transaction &txn,
                  Store &syncStateDb,
                  catalog::SyncStateKey key,
                  std::string_view value);

bool
removeSyncStateValue(Transaction &txn, Store &syncStateDb, catalog::SyncStateKey key);

bool
getSyncStateSecretValue(Transaction &txn,
                        Store &syncStateDb,
                        std::string_view secretName,
                        std::string_view &value);

std::optional<std::string>
getSyncStateSecretValue(Transaction &txn, Store &syncStateDb, std::string_view secretName);

void
putSyncStateSecretValue(Transaction &txn,
                        Store &syncStateDb,
                        std::string_view secretName,
                        std::string_view value);

bool
removeSyncStateSecretValue(Transaction &txn, Store &syncStateDb, std::string_view secretName);

} // namespace db
