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
