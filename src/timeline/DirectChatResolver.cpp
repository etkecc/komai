// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "DirectChatResolver.h"

#include <mtx/events/collections.hpp>

#include "cache/Cache.h"
#include "matrix/MatrixStateTypes.h"
#include "utils/Utils.h"

DirectChatResolver &
DirectChatResolver::instance()
{
    static DirectChatResolver inst;
    return inst;
}

void
DirectChatResolver::ensureMDirectLoaded()
{
    if (mdirectLoaded_)
        return;

    mdirectLoaded_ = true;
    mdirectMap_.clear();

    auto e = cache::getAccountData(mtx::events::EventType::Direct);
    if (!e)
        return;

    auto *event =
      std::get_if<mtx::events::AccountDataEvent<mtx::events::account_data::Direct>>(&e.value());
    if (!event)
        return;

    for (const auto &[user, rooms] : event->content.user_to_rooms) {
        QString u = QString::fromStdString(user);
        for (const auto &r : rooms)
            mdirectMap_[QString::fromStdString(r)] = u;
    }
}

QString
DirectChatResolver::computePartner(const QString &roomId)
{
    // 1. Authoritative: check m.direct account data.
    ensureMDirectLoaded();
    if (auto it = mdirectMap_.find(roomId); it != mdirectMap_.end())
        return it->second;

    // 2. Quick filter: more than 3 members is never a direct chat.
    if (cache::memberCount(roomId.toStdString()) > 3)
        return {};

    // 3. Collect non-local members.
    const auto localUser = utils::localUser();
    std::vector<RoomMember> others;
    for (const auto &member : cache::getMembers(roomId.toStdString(), 0, 4)) {
        if (member.user_id != localUser)
            others.push_back(member);
    }

    // 4. Single other member — that's the partner.
    if (others.size() == 1)
        return others[0].user_id;

    // 5. Two other members — try bot elimination.
    if (others.size() == 2) {
        bool bot0 = isLikelyBotUser(others[0].user_id, others[0].display_name);
        bool bot1 = isLikelyBotUser(others[1].user_id, others[1].display_name);
        if (bot0 && !bot1)
            return others[1].user_id;
        if (bot1 && !bot0)
            return others[0].user_id;
        // Both bots or neither — ambiguous, not a clear DM.
        return {};
    }

    return {};
}

bool
DirectChatResolver::isDirectChat(const QString &roomId)
{
    auto it = cache_.find(roomId);
    if (it == cache_.end())
        it = cache_.emplace(roomId, computePartner(roomId)).first;
    return !it->second.isEmpty();
}

QString
DirectChatResolver::directChatPartner(const QString &roomId)
{
    auto it = cache_.find(roomId);
    if (it == cache_.end())
        it = cache_.emplace(roomId, computePartner(roomId)).first;
    return it->second;
}

std::set<QString>
DirectChatResolver::reload()
{
    // Save old m.direct map.
    auto oldMap = std::move(mdirectMap_);
    mdirectMap_.clear();
    mdirectLoaded_ = false;
    ensureMDirectLoaded();

    // Clear all per-room cached results (m.direct changed, so heuristic results may differ).
    cache_.clear();

    // Compute rooms whose m.direct status changed (symmetric difference).
    std::set<QString> changed;
    for (const auto &[room, user] : oldMap) {
        auto it = mdirectMap_.find(room);
        if (it == mdirectMap_.end() || it->second != user)
            changed.insert(room);
    }
    for (const auto &[room, user] : mdirectMap_) {
        if (oldMap.find(room) == oldMap.end())
            changed.insert(room);
    }

    return changed;
}

void
DirectChatResolver::invalidateForRoomId(const QString &roomId)
{
    cache_.erase(roomId);
}

QString
DirectChatResolver::dmRoomDisplayName(const QString &roomId)
{
    auto partner = directChatPartner(roomId);
    if (partner.isEmpty())
        return {};

    // Don't override if the room has an explicit m.room.name.
    auto nameEvent = cache::getStateEvent<mtx::events::state::Name>(roomId.toStdString());
    if (nameEvent && !nameEvent->content.name.empty())
        return {};

    return cache::displayName(roomId, partner);
}

bool
DirectChatResolver::isLikelyBotUser(const QString &userId, const QString &displayName)
{
    return utils::isLikelyBotUser(userId.toStdString(), displayName.toStdString());
}
