// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "DirectChatResolver.h"

#include "chat/ChatPage.h"
#include "timeline/RoomlistModel.h"
#include "timeline/TimelineViewManager.h"
#include "utils/BotDetection.h"

namespace {
const komai::MatrixRoomSummary *
lookupMatrixRoomSummary(const QString &roomId)
{
    auto *chatPage = ChatPage::instance();
    if (!chatPage || !chatPage->timelineManager() || !chatPage->timelineManager()->rooms())
        return nullptr;

    const auto &rooms = chatPage->timelineManager()->rooms()->matrixJoinedRooms();
    const auto it     = rooms.find(roomId);
    if (it == rooms.end())
        return nullptr;

    return &it.value();
}
}

DirectChatResolver &
DirectChatResolver::instance()
{
    static DirectChatResolver inst;
    return inst;
}

void
DirectChatResolver::ensureMDirectLoaded()
{
    mdirectLoaded_ = true;
    mdirectMap_.clear();
}

QString
DirectChatResolver::computePartner(const QString &roomId)
{
    const auto *summary = lookupMatrixRoomSummary(roomId);
    if (!summary || !summary->isDirect)
        return {};

    return summary->directChatOtherUserId;
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
    mdirectMap_.clear();
    mdirectLoaded_ = true;
    cache_.clear();
    return {};
}

void
DirectChatResolver::invalidateForRoomId(const QString &roomId)
{
    cache_.erase(roomId);
}

QString
DirectChatResolver::dmRoomDisplayName(const QString &roomId)
{
    const auto *summary = lookupMatrixRoomSummary(roomId);
    if (!summary || !summary->isDirect)
        return {};

    return summary->displayName;
}

bool
DirectChatResolver::isBotRoom(const QString &roomId)
{
    const auto *summary = lookupMatrixRoomSummary(roomId);
    if (!summary || !summary->isDirect)
        return false;

    if (summary->isBotRoom)
        return true;

    return isLikelyBotUser(summary->directChatOtherUserId, summary->displayName);
}

bool
DirectChatResolver::isLikelyBotUser(const QString &userId, const QString &displayName)
{
    return utils::isLikelyBotUser(userId.toStdString(), displayName.toStdString());
}
