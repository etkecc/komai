// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <optional>
#include <string_view>
#include <vector>

#include <QString>
#include <QStringList>
#include <QVector>

#include <mtx/responses/sync.hpp>

namespace komai {

struct JoinedRoomSyncUpdate
{
    QString roomId;
    QStringList typingUsers;
    bool tagsChanged          = false;
    bool spaceInfoChanged     = false;
    bool ownMembershipChanged = false;

    // Non-owning view into the source sync payload. It is only valid while the surrounding
    // SyncUpdate is being applied synchronously.
    const mtx::responses::JoinedRoom *room = nullptr;
};

struct SyncUpdate
{
    bool directChatsChanged = false;
    std::optional<QVector<QString>> ignoredUsers;
    QVector<QString> presenceUserIds;
    std::vector<JoinedRoomSyncUpdate> joinedRooms;
    QVector<QString> leftRoomIds;
    QVector<QString> invitedRoomIds;
};

SyncUpdate
buildSyncUpdate(const mtx::responses::Sync &sync, std::string_view localUserId);

} // namespace komai
