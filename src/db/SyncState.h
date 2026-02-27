// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "db/Catalog.h"
#include "db/Json.h"

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

template<typename T>
bool
getSyncStateJsonValue(Transaction &txn, Store &syncStateDb, catalog::SyncStateKey key, T &value)
{
    return getJsonValue(txn, syncStateDb, catalog::syncStateKey(key), value);
}

template<typename T>
std::optional<T>
getSyncStateJsonValue(Transaction &txn, Store &syncStateDb, catalog::SyncStateKey key)
{
    return getJsonValue<T>(txn, syncStateDb, catalog::syncStateKey(key));
}

template<typename T>
void
putSyncStateJsonValue(Transaction &txn,
                      Store &syncStateDb,
                      catalog::SyncStateKey key,
                      const T &value)
{
    putJsonValue(txn, syncStateDb, catalog::syncStateKey(key), value);
}

} // namespace db
