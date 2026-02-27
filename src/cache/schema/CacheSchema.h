// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <optional>
#include <string>
#include <string_view>

#include "db/storage/Open.h"
#include "db/storage/Serde.h"
#include "db/storage/SyncState.h"

namespace cache::schema {

enum class GlobalDb
{
    Rooms,
    Invites,
    SpacesParents,
    SpacesChildren,
    SyncState,
    ReadReceipts,
    Notifications,
    Presence,
    EncryptedRooms,
    EventExpirationBgJob,
    InboundMegolmSessions,
    OutboundMegolmSessions,
    MegolmSessionsData,
    OlmSessions,
    UserKeys,
    Verified,
    PendingReceipts,
};

enum class RoomDb
{
    Events,
    EventOrder,
    EventToOrder,
    MessageToOrder,
    OrderToMessage,
    Pending,
    Related,
    InviteState,
    InviteMembers,
    State,
    StatesKey,
    AccountData,
    Members,
};

enum class SyncStateKey
{
    NextBatch,
    OlmAccount,
    CacheFormatVersion,
    CurrentOnlineBackupVersion,
};

inline constexpr db::catalog::GlobalDb
toDb(GlobalDb db) noexcept
{
    switch (db) {
    case GlobalDb::Rooms:
        return db::catalog::GlobalDb::Rooms;
    case GlobalDb::Invites:
        return db::catalog::GlobalDb::Invites;
    case GlobalDb::SpacesParents:
        return db::catalog::GlobalDb::SpacesParents;
    case GlobalDb::SpacesChildren:
        return db::catalog::GlobalDb::SpacesChildren;
    case GlobalDb::SyncState:
        return db::catalog::GlobalDb::SyncState;
    case GlobalDb::ReadReceipts:
        return db::catalog::GlobalDb::ReadReceipts;
    case GlobalDb::Notifications:
        return db::catalog::GlobalDb::Notifications;
    case GlobalDb::Presence:
        return db::catalog::GlobalDb::Presence;
    case GlobalDb::EncryptedRooms:
        return db::catalog::GlobalDb::EncryptedRooms;
    case GlobalDb::EventExpirationBgJob:
        return db::catalog::GlobalDb::EventExpirationBgJob;
    case GlobalDb::InboundMegolmSessions:
        return db::catalog::GlobalDb::InboundMegolmSessions;
    case GlobalDb::OutboundMegolmSessions:
        return db::catalog::GlobalDb::OutboundMegolmSessions;
    case GlobalDb::MegolmSessionsData:
        return db::catalog::GlobalDb::MegolmSessionsData;
    case GlobalDb::OlmSessions:
        return db::catalog::GlobalDb::OlmSessions;
    case GlobalDb::UserKeys:
        return db::catalog::GlobalDb::UserKeys;
    case GlobalDb::Verified:
        return db::catalog::GlobalDb::Verified;
    case GlobalDb::PendingReceipts:
        return db::catalog::GlobalDb::PendingReceipts;
    default:
        return db::catalog::GlobalDb::Rooms;
    }
}

inline constexpr db::catalog::RoomDb
toDb(RoomDb db) noexcept
{
    switch (db) {
    case RoomDb::Events:
        return db::catalog::RoomDb::Events;
    case RoomDb::EventOrder:
        return db::catalog::RoomDb::EventOrder;
    case RoomDb::EventToOrder:
        return db::catalog::RoomDb::EventToOrder;
    case RoomDb::MessageToOrder:
        return db::catalog::RoomDb::MessageToOrder;
    case RoomDb::OrderToMessage:
        return db::catalog::RoomDb::OrderToMessage;
    case RoomDb::Pending:
        return db::catalog::RoomDb::Pending;
    case RoomDb::Related:
        return db::catalog::RoomDb::Related;
    case RoomDb::InviteState:
        return db::catalog::RoomDb::InviteState;
    case RoomDb::InviteMembers:
        return db::catalog::RoomDb::InviteMembers;
    case RoomDb::State:
        return db::catalog::RoomDb::State;
    case RoomDb::StatesKey:
        return db::catalog::RoomDb::StatesKey;
    case RoomDb::AccountData:
        return db::catalog::RoomDb::AccountData;
    case RoomDb::Members:
        return db::catalog::RoomDb::Members;
    default:
        return db::catalog::RoomDb::Events;
    }
}

inline constexpr db::catalog::SyncStateKey
toDb(SyncStateKey key) noexcept
{
    switch (key) {
    case SyncStateKey::NextBatch:
        return db::catalog::SyncStateKey::NextBatch;
    case SyncStateKey::OlmAccount:
        return db::catalog::SyncStateKey::OlmAccount;
    case SyncStateKey::CacheFormatVersion:
        return db::catalog::SyncStateKey::CacheFormatVersion;
    case SyncStateKey::CurrentOnlineBackupVersion:
        return db::catalog::SyncStateKey::CurrentOnlineBackupVersion;
    default:
        return db::catalog::SyncStateKey::NextBatch;
    }
}

inline std::string_view
syncStateKey(SyncStateKey key) noexcept
{
    return db::catalog::syncStateKey(toDb(key));
}

inline std::string
roomName(std::string_view roomId, RoomDb db)
{
    return db::catalog::roomName(roomId, toDb(db));
}

inline std::string
roomName(std::string_view roomId, db::catalog::RoomDb db)
{
    return db::catalog::roomName(roomId, db);
}

inline db::Store
openGlobalStore(db::Database &database, db::Transaction &txn, GlobalDb store, bool create = true)
{
    return db::openGlobalStore(database, txn, toDb(store), create);
}

inline db::Store
openRoomStore(db::Database &database,
              db::Transaction &txn,
              std::string_view roomId,
              RoomDb store,
              bool create = true)
{
    return db::openRoomStore(database, txn, roomId, toDb(store), create);
}

} // namespace cache::schema

namespace cache::sync_state {

inline std::optional<std::string>
getValue(db::Transaction &txn, db::Store &syncStateDb, schema::SyncStateKey key)
{
    return db::getSyncStateValue(txn, syncStateDb, schema::toDb(key));
}

inline void
putValue(db::Transaction &txn,
         db::Store &syncStateDb,
         schema::SyncStateKey key,
         std::string_view value)
{
    db::putSyncStateValue(txn, syncStateDb, schema::toDb(key), value);
}

inline bool
removeValue(db::Transaction &txn, db::Store &syncStateDb, schema::SyncStateKey key)
{
    return db::removeSyncStateValue(txn, syncStateDb, schema::toDb(key));
}

inline std::optional<std::string>
getNextBatchToken(db::Transaction &txn, db::Store &syncStateDb)
{
    return getValue(txn, syncStateDb, schema::SyncStateKey::NextBatch);
}

inline void
putNextBatchToken(db::Transaction &txn, db::Store &syncStateDb, std::string_view token)
{
    putValue(txn, syncStateDb, schema::SyncStateKey::NextBatch, token);
}

inline std::optional<std::string>
getCacheFormatVersion(db::Transaction &txn, db::Store &syncStateDb)
{
    return getValue(txn, syncStateDb, schema::SyncStateKey::CacheFormatVersion);
}

inline void
putCacheFormatVersion(db::Transaction &txn, db::Store &syncStateDb, std::string_view version)
{
    putValue(txn, syncStateDb, schema::SyncStateKey::CacheFormatVersion, version);
}

inline std::optional<std::string>
getOlmAccount(db::Transaction &txn, db::Store &syncStateDb)
{
    return getValue(txn, syncStateDb, schema::SyncStateKey::OlmAccount);
}

inline void
putOlmAccount(db::Transaction &txn, db::Store &syncStateDb, std::string_view account)
{
    putValue(txn, syncStateDb, schema::SyncStateKey::OlmAccount, account);
}

template<typename T>
std::optional<T>
getCurrentOnlineBackupVersion(db::Transaction &txn, db::Store &syncStateDb)
{
    return db::getSyncStateJsonValue<T>(
      txn, syncStateDb, schema::toDb(schema::SyncStateKey::CurrentOnlineBackupVersion));
}

template<typename T>
void
putCurrentOnlineBackupVersion(db::Transaction &txn, db::Store &syncStateDb, const T &value)
{
    db::putSyncStateJsonValue(
      txn, syncStateDb, schema::toDb(schema::SyncStateKey::CurrentOnlineBackupVersion), value);
}

inline bool
removeCurrentOnlineBackupVersion(db::Transaction &txn, db::Store &syncStateDb)
{
    return removeValue(txn, syncStateDb, schema::SyncStateKey::CurrentOnlineBackupVersion);
}

inline bool
getSecretValue(db::Transaction &txn,
               db::Store &syncStateDb,
               std::string_view secretName,
               std::string_view &value)
{
    return db::getSyncStateSecretValue(txn, syncStateDb, secretName, value);
}

inline std::optional<std::string>
getSecretValue(db::Transaction &txn, db::Store &syncStateDb, std::string_view secretName)
{
    return db::getSyncStateSecretValue(txn, syncStateDb, secretName);
}

inline void
putSecretValue(db::Transaction &txn,
               db::Store &syncStateDb,
               std::string_view secretName,
               std::string_view value)
{
    db::putSyncStateSecretValue(txn, syncStateDb, secretName, value);
}

inline bool
removeSecretValue(db::Transaction &txn, db::Store &syncStateDb, std::string_view secretName)
{
    return db::removeSyncStateSecretValue(txn, syncStateDb, secretName);
}

} // namespace cache::sync_state
