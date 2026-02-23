// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <utility>

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
    LegacyMentions,
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

std::string
syncStateSecretKey(std::string_view secretName);

std::string_view
legacyOlmSessionsPrefixV1() noexcept;

std::string_view
legacyOlmSessionsPrefixV2() noexcept;

bool
isLegacyOlmShardV1(std::string_view dbName) noexcept;

bool
isLegacyOlmShardV2(std::string_view dbName) noexcept;

std::string
legacyOlmShardV2NameFromV1(std::string_view dbNameV1);

std::optional<std::string_view>
legacyOlmCurveFromV2Name(std::string_view dbNameV2) noexcept;

std::string
olmSessionKey(std::string_view curve25519, std::string_view sessionId);

std::pair<std::string_view, std::string_view>
splitOlmSessionKey(std::string_view key) noexcept;

std::string
stateEventIndexValue(std::string_view stateKey, std::string_view eventId);

std::pair<std::string_view, std::string_view>
splitStateEventIndexValue(std::string_view value) noexcept;

} // namespace db::catalog
