// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "matrix/MatrixSyncUpdate.h"

#include <algorithm>

namespace {

using IgnoredUsersEvent = mtx::events::AccountDataEvent<mtx::events::account_data::IgnoredUsers>;
using DirectChatsEvent  = mtx::events::AccountDataEvent<mtx::events::account_data::Direct>;
using TagsEvent         = mtx::events::AccountDataEvent<mtx::events::account_data::Tags>;

QVector<QString>
convertIgnoredUsers(const IgnoredUsersEvent &event)
{
    QVector<QString> users;
    users.reserve(static_cast<qsizetype>(event.content.users.size()));

    for (const auto &user : event.content.users)
        users.push_back(QString::fromStdString(user.id));

    return users;
}

template<class EventCollection>
bool
containsSpaceInfoChange(const EventCollection &events)
{
    return std::ranges::any_of(events, [](const auto &event) {
        return std::holds_alternative<mtx::events::StateEvent<mtx::events::state::space::Child>>(
                 event) ||
               std::holds_alternative<mtx::events::StateEvent<mtx::events::state::space::Parent>>(
                 event);
    });
}

template<class EventCollection>
bool
containsOwnMembershipChange(const EventCollection &events, std::string_view localUserId)
{
    return std::ranges::any_of(events, [&localUserId](const auto &event) {
        if (auto member = std::get_if<mtx::events::StateEvent<mtx::events::state::Member>>(&event))
            return member->state_key == localUserId;

        return false;
    });
}

QStringList
extractTypingUsers(const mtx::responses::JoinedRoom &room, std::string_view localUserId)
{
    QStringList typingUsers;

    for (const auto &event : room.ephemeral.events) {
        if (auto typing =
              std::get_if<mtx::events::EphemeralEvent<mtx::events::ephemeral::Typing>>(&event)) {
            typingUsers.clear();
            typingUsers.reserve(static_cast<qsizetype>(typing->content.user_ids.size()));

            for (const auto &userId : typing->content.user_ids) {
                if (userId != localUserId)
                    typingUsers.push_back(QString::fromStdString(userId));
            }
        }
    }

    return typingUsers;
}

} // namespace

namespace komai {

SyncUpdate
buildSyncUpdate(const mtx::responses::Sync &sync, std::string_view localUserId)
{
    SyncUpdate update;

    update.presenceUserIds.reserve(static_cast<qsizetype>(sync.presence.size()));
    update.joinedRooms.reserve(sync.rooms.join.size());
    update.leftRoomIds.reserve(static_cast<qsizetype>(sync.rooms.leave.size()));
    update.invitedRoomIds.reserve(static_cast<qsizetype>(sync.rooms.invite.size()));

    for (const auto &event : sync.account_data.events) {
        if (!update.directChatsChanged && std::holds_alternative<DirectChatsEvent>(event))
            update.directChatsChanged = true;

        if (!update.ignoredUsers) {
            if (auto ignored = std::get_if<IgnoredUsersEvent>(&event))
                update.ignoredUsers = convertIgnoredUsers(*ignored);
        }
    }

    for (const auto &presence : sync.presence)
        update.presenceUserIds.push_back(QString::fromStdString(presence.sender));

    for (const auto &[roomId, room] : sync.rooms.join) {
        JoinedRoomSyncUpdate roomUpdate;
        roomUpdate.room        = &room;
        roomUpdate.roomId      = QString::fromStdString(roomId);
        roomUpdate.typingUsers = extractTypingUsers(room, localUserId);
        roomUpdate.tagsChanged =
          std::ranges::any_of(room.account_data.events, [](const auto &event) {
              return std::holds_alternative<TagsEvent>(event);
          });
        roomUpdate.spaceInfoChanged = containsSpaceInfoChange(room.state.events) ||
                                      containsSpaceInfoChange(room.timeline.events);
        roomUpdate.ownMembershipChanged =
          containsOwnMembershipChange(room.state.events, localUserId) ||
          containsOwnMembershipChange(room.timeline.events, localUserId);

        update.joinedRooms.push_back(std::move(roomUpdate));
    }

    for (const auto &[roomId, room] : sync.rooms.leave) {
        (void)room;
        update.leftRoomIds.push_back(QString::fromStdString(roomId));
    }

    for (const auto &[roomId, room] : sync.rooms.invite) {
        (void)room;
        update.invitedRoomIds.push_back(QString::fromStdString(roomId));
    }

    return update;
}

NotificationSyncUpdate
buildNotificationSyncUpdate(const mtx::responses::Sync &sync)
{
    NotificationSyncUpdate update;

    for (const auto &[roomId, room] : sync.rooms.join) {
        (void)roomId;
        update.notificationCount +=
          static_cast<unsigned int>(room.unread_notifications.notification_count);
    }

    return update;
}

} // namespace komai
