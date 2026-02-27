// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "cache/Cache.h"
#include "cache/core/Cache_p.h"

#include <QChar>

#include <nlohmann/json.hpp>

#include <spdlog/logger.h>

#include "cache/api/CacheApiContext.h"

QString
Cache::displayName(const QString &room_id, const QString &user_id)
{
    return QString::fromStdString(displayName(room_id.toStdString(), user_id.toStdString()));
}

static bool
isDisplaynameSafe(const std::string &s)
{
    const auto str = QString::fromStdString(s);

    for (QChar c : str) {
        if (c.isPrint() && !c.isSpace())
            return false;
    }

    return true;
}

std::string
Cache::displayName(const std::string &room_id, const std::string &user_id)
{
    if (auto info = getMember(room_id, user_id); info && !isDisplaynameSafe(info->name))
        return info->name;

    return user_id;
}

QString
Cache::avatarUrl(const QString &room_id, const QString &user_id)
{
    if (auto info = getMember(room_id.toStdString(), user_id.toStdString());
        info && !info->avatar_url.empty())
        return QString::fromStdString(info->avatar_url);

    return QString();
}

mtx::events::presence::Presence
Cache::presence(const std::string &user_id)
{
    mtx::events::presence::Presence presence_{};
    presence_.presence = mtx::presence::PresenceState::offline;

    if (user_id.empty())
        return presence_;

    auto txn = ro_txn(storage());
    try {
        if (auto val =
              db::getJsonValue<mtx::events::presence::Presence>(txn, db->presence, user_id))
            presence_ = std::move(*val);
    } catch (const nlohmann::json::exception &e) {
        cache::activeLoggers().db->warn(
          "failed to parse presence entry for {}: {}", user_id, e.what());
    }

    return presence_;
}
