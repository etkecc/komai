// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "db/SyncState.h"

#include "db/DbTypes.h"

namespace db {

bool
getSyncStateValue(Txn &txn, Store &syncStateDb, catalog::SyncStateKey key, std::string_view &value)
{
    return syncStateDb.get(txn, catalog::syncStateKey(key), value);
}

std::optional<std::string>
getSyncStateValue(Txn &txn, Store &syncStateDb, catalog::SyncStateKey key)
{
    std::string_view value;
    if (!getSyncStateValue(txn, syncStateDb, key, value))
        return std::nullopt;

    return std::string(value);
}

void
putSyncStateValue(Txn &txn, Store &syncStateDb, catalog::SyncStateKey key, std::string_view value)
{
    syncStateDb.put(txn, catalog::syncStateKey(key), value);
}

bool
removeSyncStateValue(Txn &txn, Store &syncStateDb, catalog::SyncStateKey key)
{
    return syncStateDb.del(txn, catalog::syncStateKey(key));
}

std::optional<std::string>
getNextBatchToken(Txn &txn, Store &syncStateDb)
{
    return getSyncStateValue(txn, syncStateDb, catalog::SyncStateKey::NextBatch);
}

void
putNextBatchToken(Txn &txn, Store &syncStateDb, std::string_view token)
{
    putSyncStateValue(txn, syncStateDb, catalog::SyncStateKey::NextBatch, token);
}

std::optional<std::string>
getCacheFormatVersion(Txn &txn, Store &syncStateDb)
{
    return getSyncStateValue(txn, syncStateDb, catalog::SyncStateKey::CacheFormatVersion);
}

void
putCacheFormatVersion(Txn &txn, Store &syncStateDb, std::string_view version)
{
    putSyncStateValue(txn, syncStateDb, catalog::SyncStateKey::CacheFormatVersion, version);
}

std::optional<std::string>
getOlmAccount(Txn &txn, Store &syncStateDb)
{
    return getSyncStateValue(txn, syncStateDb, catalog::SyncStateKey::OlmAccount);
}

void
putOlmAccount(Txn &txn, Store &syncStateDb, std::string_view account)
{
    putSyncStateValue(txn, syncStateDb, catalog::SyncStateKey::OlmAccount, account);
}

bool
getSyncStateSecretValue(Txn &txn,
                        Store &syncStateDb,
                        std::string_view secretName,
                        std::string_view &value)
{
    return syncStateDb.get(txn, catalog::syncStateSecretKey(secretName), value);
}

std::optional<std::string>
getSyncStateSecretValue(Txn &txn, Store &syncStateDb, std::string_view secretName)
{
    std::string_view value;
    if (!getSyncStateSecretValue(txn, syncStateDb, secretName, value))
        return std::nullopt;

    return std::string(value);
}

void
putSyncStateSecretValue(Txn &txn,
                        Store &syncStateDb,
                        std::string_view secretName,
                        std::string_view value)
{
    syncStateDb.put(txn, catalog::syncStateSecretKey(secretName), value);
}

bool
removeSyncStateSecretValue(Txn &txn, Store &syncStateDb, std::string_view secretName)
{
    return syncStateDb.del(txn, catalog::syncStateSecretKey(secretName));
}

} // namespace db
