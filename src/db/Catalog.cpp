// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "db/Catalog.h"

namespace db::catalog {

std::string_view
globalName(GlobalDb db) noexcept
{
    switch (db) {
    case GlobalDb::Rooms:
        return "rooms";
    case GlobalDb::Invites:
        return "invites";
    case GlobalDb::SpacesParents:
        return "space_parents";
    case GlobalDb::SpacesChildren:
        return "space_children";
    case GlobalDb::SyncState:
        return "sync_state";
    case GlobalDb::ReadReceipts:
        return "read_receipts";
    case GlobalDb::Notifications:
        return "sent_notifications";
    case GlobalDb::Presence:
        return "presence";
    case GlobalDb::EncryptedRooms:
        return "encrypted_rooms";
    case GlobalDb::EventExpirationBgJob:
        return "event_expiration_bg_job";
    case GlobalDb::InboundMegolmSessions:
        return "inbound_megolm_sessions";
    case GlobalDb::OutboundMegolmSessions:
        return "outbound_megolm_sessions";
    case GlobalDb::MegolmSessionsData:
        return "megolm_sessions_data_db";
    case GlobalDb::OlmSessions:
        return "olm_sessions.v3";
    case GlobalDb::UserKeys:
        return "user_key";
    case GlobalDb::Verified:
        return "verified";
    case GlobalDb::PendingReceipts:
        return "pending_receipts";
    }

    return {};
}

std::string_view
roomSuffix(RoomDb db) noexcept
{
    switch (db) {
    case RoomDb::Events:
        return "/events";
    case RoomDb::EventOrder:
        return "/event_order";
    case RoomDb::EventToOrder:
        return "/event2order";
    case RoomDb::MessageToOrder:
        return "/msg2order";
    case RoomDb::OrderToMessage:
        return "/order2msg";
    case RoomDb::Pending:
        return "/pending";
    case RoomDb::Related:
        return "/related";
    case RoomDb::InviteState:
        return "/invite_state";
    case RoomDb::InviteMembers:
        return "/invite_members";
    case RoomDb::State:
        return "/state";
    case RoomDb::StatesKey:
        return "/states_key";
    case RoomDb::AccountData:
        return "/account_data";
    case RoomDb::Members:
        return "/members";
    case RoomDb::LegacyMessages:
        return "/messages";
    case RoomDb::LegacyMentions:
        return "/mentions";
    case RoomDb::LegacyStateByKey:
        return "/state_by_key";
    }

    return {};
}

bool
hasRoomSuffix(std::string_view dbName, RoomDb db) noexcept
{
    return dbName.ends_with(roomSuffix(db));
}

std::string
roomName(std::string_view roomId, RoomDb db)
{
    const auto suffix = roomSuffix(db);

    std::string name;
    name.reserve(roomId.size() + suffix.size());
    name.append(roomId);
    name.append(suffix);
    return name;
}

std::string_view
syncStateKey(SyncStateKey key) noexcept
{
    switch (key) {
    case SyncStateKey::NextBatch:
        return "next_batch";
    case SyncStateKey::OlmAccount:
        return "olm_account";
    case SyncStateKey::CacheFormatVersion:
        return "cache_format_version";
    case SyncStateKey::CurrentOnlineBackupVersion:
        return "current_online_backup_version";
    }

    return {};
}

std::string_view
legacyOlmSessionsPrefixV1() noexcept
{
    return "olm_sessions/";
}

std::string_view
legacyOlmSessionsPrefixV2() noexcept
{
    return "olm_sessions.v2/";
}

bool
isLegacyOlmShardV1(std::string_view dbName) noexcept
{
    return dbName.starts_with(legacyOlmSessionsPrefixV1());
}

bool
isLegacyOlmShardV2(std::string_view dbName) noexcept
{
    return dbName.starts_with(legacyOlmSessionsPrefixV2());
}

std::string
legacyOlmShardV2NameFromV1(std::string_view dbNameV1)
{
    if (!isLegacyOlmShardV1(dbNameV1))
        return std::string(dbNameV1);

    std::string name;
    name.reserve(dbNameV1.size() + 3);
    name.append("olm_sessions.v2");
    name.append(dbNameV1.substr(legacyOlmSessionsPrefixV1().size() - 1));
    return name;
}

std::optional<std::string_view>
legacyOlmCurveFromV2Name(std::string_view dbNameV2) noexcept
{
    if (!isLegacyOlmShardV2(dbNameV2))
        return std::nullopt;

    return dbNameV2.substr(legacyOlmSessionsPrefixV2().size());
}

} // namespace db::catalog
