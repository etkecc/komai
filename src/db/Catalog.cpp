// SPDX-FileCopyrightText: Nheko Contributors
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

std::string
syncStateSecretKey(std::string_view secretName)
{
    std::string key;
    key.reserve(7 + secretName.size());
    key.append("secret.");
    key.append(secretName);
    return key;
}

std::string
olmSessionKey(std::string_view curve25519, std::string_view sessionId)
{
    std::string combined(curve25519.size() + 1 + sessionId.size(), '\0');
    combined.replace(0, curve25519.size(), curve25519);
    combined.replace(curve25519.size() + 1, sessionId.size(), sessionId);
    return combined;
}

std::pair<std::string_view, std::string_view>
splitOlmSessionKey(std::string_view key) noexcept
{
    const auto separator = key.find('\0');
    return std::pair(key.substr(0, separator), key.substr(separator + 1));
}

std::string
stateEventIndexValue(std::string_view stateKey, std::string_view eventId)
{
    std::string combined(stateKey.size() + 1 + eventId.size(), '\0');
    combined.replace(0, stateKey.size(), stateKey);
    combined.replace(stateKey.size() + 1, eventId.size(), eventId);
    return combined;
}

std::pair<std::string_view, std::string_view>
splitStateEventIndexValue(std::string_view value) noexcept
{
    const auto separator = value.rfind('\0');
    if (separator == std::string_view::npos)
        return std::pair(value, std::string_view{});

    return std::pair(value.substr(0, separator), value.substr(separator + 1));
}

} // namespace db::catalog
