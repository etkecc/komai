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

std::optional<std::string>
getNextBatchToken(Transaction &txn, Store &syncStateDb);

void
putNextBatchToken(Transaction &txn, Store &syncStateDb, std::string_view token);

std::optional<std::string>
getCacheFormatVersion(Transaction &txn, Store &syncStateDb);

void
putCacheFormatVersion(Transaction &txn, Store &syncStateDb, std::string_view version);

std::optional<std::string>
getOlmAccount(Transaction &txn, Store &syncStateDb);

void
putOlmAccount(Transaction &txn, Store &syncStateDb, std::string_view account);

bool
removeCurrentOnlineBackupVersion(Transaction &txn, Store &syncStateDb);

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

template<typename T>
bool
getCurrentOnlineBackupVersion(Transaction &txn, Store &syncStateDb, T &value)
{
    return getSyncStateJsonValue(
      txn, syncStateDb, catalog::SyncStateKey::CurrentOnlineBackupVersion, value);
}

template<typename T>
std::optional<T>
getCurrentOnlineBackupVersion(Transaction &txn, Store &syncStateDb)
{
    return getSyncStateJsonValue<T>(
      txn, syncStateDb, catalog::SyncStateKey::CurrentOnlineBackupVersion);
}

template<typename T>
void
putCurrentOnlineBackupVersion(Transaction &txn, Store &syncStateDb, const T &value)
{
    putSyncStateJsonValue(
      txn, syncStateDb, catalog::SyncStateKey::CurrentOnlineBackupVersion, value);
}

} // namespace db
