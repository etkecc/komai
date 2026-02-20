// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <string>
#include <string_view>

namespace db::catalog {

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
    LegacyMessages,
    LegacyStateByKey,
};

enum class SyncStateKey
{
    NextBatch,
    OlmAccount,
    CacheFormatVersion,
    CurrentOnlineBackupVersion,
};

std::string_view
globalName(GlobalDb db) noexcept;

std::string_view
roomSuffix(RoomDb db) noexcept;

bool
hasRoomSuffix(std::string_view dbName, RoomDb db) noexcept;

std::string
roomName(std::string_view roomId, RoomDb db);

std::string_view
syncStateKey(SyncStateKey key) noexcept;

} // namespace db::catalog
