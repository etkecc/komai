// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cache/Cache.h"
#include "cache/core/Cache_p.h"

#include <string_view>

#include <nlohmann/json.hpp>
#include <spdlog/logger.h>

#include "cache/api/CacheApiContext.h"

QString
MatrixStore::getInviteRoomName(db::Transaction &txn, db::Store &statesdb, db::Store &membersdb)
{
    using namespace mtx::events;
    using namespace mtx::events::state;

    try {
        if (auto msg = db::getJsonValue<StrippedEvent<state::Name>>(
              txn, statesdb, to_string(mtx::events::EventType::RoomName))) {
            return QString::fromStdString(msg->content.name);
        }
    } catch (const nlohmann::json::exception &e) {
        cache::activeLoggers().db->warn("failed to parse m.room.name event: {}", e.what());
    }

    const auto localUserId = localUserId_.toStdString();
    QString memberName;
    bool foundMemberName = false;
    db::forEachEntry(txn, membersdb, [&](std::string_view user_id, std::string_view member_data) {
        if (user_id == localUserId)
            return true;

        try {
            MemberInfo tmp  = cache::codec::parseMemberInfo(member_data);
            memberName      = QString::fromStdString(tmp.name);
            foundMemberName = true;
            return false;
        } catch (const nlohmann::json::exception &e) {
            cache::activeLoggers().db->warn("failed to parse member info: {}", e.what());
        }
        return true;
    });

    if (foundMemberName)
        return memberName;

    return tr("Empty Room");
}

QString
MatrixStore::getInviteRoomAvatarUrl(db::Transaction &txn, db::Store &statesdb, db::Store &membersdb)
{
    using namespace mtx::events;
    using namespace mtx::events::state;

    try {
        if (auto msg = db::getJsonValue<StrippedEvent<state::Avatar>>(
              txn, statesdb, to_string(mtx::events::EventType::RoomAvatar))) {
            return QString::fromStdString(msg->content.url);
        }
    } catch (const nlohmann::json::exception &e) {
        cache::activeLoggers().db->warn("failed to parse m.room.avatar event: {}", e.what());
    }

    const auto localUserId = localUserId_.toStdString();
    QString avatarUrl;
    bool foundAvatarUrl = false;
    db::forEachEntry(txn, membersdb, [&](std::string_view user_id, std::string_view member_data) {
        if (user_id == localUserId)
            return true;

        try {
            MemberInfo tmp = cache::codec::parseMemberInfo(member_data);
            avatarUrl      = QString::fromStdString(tmp.avatar_url);
            foundAvatarUrl = true;
            return false;
        } catch (const nlohmann::json::exception &e) {
            cache::activeLoggers().db->warn("failed to parse member info: {}", e.what());
        }
        return true;
    });

    if (foundAvatarUrl)
        return avatarUrl;

    return QString();
}

QString
MatrixStore::getInviteRoomTopic(db::Transaction &txn, db::Store &db_)
{
    using namespace mtx::events;
    using namespace mtx::events::state;

    try {
        if (auto msg = db::getJsonValue<StrippedEvent<Topic>>(
              txn, db_, to_string(mtx::events::EventType::RoomTopic))) {
            return QString::fromStdString(msg->content.topic);
        }
    } catch (const nlohmann::json::exception &e) {
        cache::activeLoggers().db->warn("failed to parse m.room.topic event: {}", e.what());
    }

    return QString();
}

bool
MatrixStore::getInviteRoomIsSpace(db::Transaction &txn, db::Store &db_)
{
    using namespace mtx::events;
    using namespace mtx::events::state;

    try {
        if (auto msg = db::getJsonValue<StrippedEvent<Create>>(
              txn, db_, to_string(mtx::events::EventType::RoomCreate))) {
            return msg->content.type == mtx::events::state::room_type::space;
        }
    } catch (const nlohmann::json::exception &e) {
        cache::activeLoggers().db->warn("failed to parse m.room.topic event: {}", e.what());
    }

    return false;
}
