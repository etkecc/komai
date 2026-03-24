// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "RoomlistModel.h"

#include "DirectChatResolver.h"
#include "TimelineModel.h"
#include "cache/Cache.h"
#include "chat/ChatPage.h"
#include "logging/Logging.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "ui/MainWindow.h"
#include "voip/CallManager.h"

void
RoomlistModel::emitRoomRowUpdate(const QString &room_id)
{
    if (auto idx = roomidToIndex(room_id); idx != -1) {
        emit dataChanged(index(idx),
                         index(idx),
                         {Roles::AvatarUrl,
                          Roles::RoomName,
                          Roles::LastMessage,
                          Roles::Time,
                          Roles::Timestamp,
                          Roles::HasDraft,
                          Roles::DraftPreview,
                          Roles::NotificationCount,
                          Roles::HasLoudNotification,
                          Roles::IsInvite,
                          Roles::IsSpace,
                          Roles::Tags,
                          Roles::IsEncrypted});
    }
}

void
RoomlistModel::syncJoinedRoom(const komai::JoinedRoomSyncUpdate &roomUpdate)
{
    auto qroomid = roomUpdate.roomId;
    const bool shouldMaterialize =
      models.contains(qroomid) || pendingCurrentRoomId_ == qroomid ||
      (currentRoomPreview_ && currentRoomPreview_->roomid() == qroomid);

    if (shouldMaterialize) {
        // addRoom will only add the room, if it doesn't exist
        addRoom(qroomid, false, "sync.materialized_room");
        const auto &room_model = models.value(qroomid);

        if (!room_model.isNull()) {
            // WORKAROUND(Nico): This is not a lambda, but clazy on alpine currently doesn't
            // believe us
            connect(room_model.data(),
                    &TimelineModel::newCallEvent,
                    ChatPage::instance()->callManager(),
                    &CallManager::syncEvent,
                    Qt::UniqueConnection); // clazy:exclude=lambda-unique-connection

            room_model->sync(roomUpdate);

            if (ChatPage::instance()->userSettings()->timelineTypingShowEnabled())
                room_model->updateTypingUsers(roomUpdate.typingUsers);
        }
    } else {
        const int existingIdx = roomidToIndex(qroomid);
        if (existingIdx == -1) {
            beginInsertRows(QModelIndex(), (int)roomids.size(), (int)roomids.size());
            roomids.push_back(qroomid);
            endInsertRows();
        }

        if (invites.contains(qroomid))
            invites.remove(qroomid);
        if (previewedRooms.contains(qroomid))
            previewedRooms.remove(qroomid);
    }

    refreshCachedRoomMetadata(qroomid);
    emitRoomRowUpdate(qroomid);
}

void
RoomlistModel::syncLeftRoom(const QString &qroomid)
{
    if ((currentRoom_ && currentRoom_->roomId() == qroomid) ||
        (currentRoomPreview_ && currentRoomPreview_->roomid() == qroomid))
        resetCurrentRoom();

    auto idx = this->roomidToIndex(qroomid);
    if (idx != -1) {
        beginRemoveRows(QModelIndex(), idx, idx);
        roomids.erase(roomids.begin() + idx);
        endRemoveRows();
    }

    removeRoomState(qroomid);
}

void
RoomlistModel::syncInvitedRoom(const QString &qroomid)
{
    auto invite = cache::invite(qroomid.toStdString());
    if (!invite)
        return;

    if (invites.contains(qroomid)) {
        invites[qroomid] = *invite;
        auto idx         = roomidToIndex(qroomid);
        emit dataChanged(index(idx), index(idx));
    } else {
        beginInsertRows(QModelIndex(), (int)roomids.size(), (int)roomids.size());
        invites.insert(qroomid, *invite);
        roomids.push_back(std::move(qroomid));
        endInsertRows();
    }
}

void
RoomlistModel::sync(const komai::SyncUpdate &sync)
{
    if (sync.directChatsChanged) {
        auto changedRooms = DirectChatResolver::instance().reload();
        for (const auto &r : changedRooms) {
            if (auto idx = roomidToIndex(r); idx != -1)
                emit dataChanged(index(idx), index(idx), {IsDirect, DirectChatOtherUserId});
            if (auto room = models.value(r); !room.isNull()) {
                emit room->isDirectChanged();
                emit room->directChatOtherUserIdChanged();
            }
        }
    }

    for (const auto &roomUpdate : sync.joinedRooms)
        syncJoinedRoom(roomUpdate);

    for (const auto &roomId : sync.leftRoomIds)
        syncLeftRoom(roomId);

    for (const auto &roomId : sync.invitedRoomIds)
        syncInvitedRoom(roomId);
}

void
RoomlistModel::initializeRooms()
{
    beginResetModel();
    resetRoomCollections(false);

    DirectChatResolver::instance().reload();

    invites               = cache::invites();
    const int inviteCount = invites.size();
    for (auto id = invites.keyBegin(); id != invites.keyEnd(); ++id) {
        roomids.push_back(*id);
    }

    const auto joinedRooms = cache::roomIds();
    for (const auto &id : joinedRooms) {
        roomids.push_back(id);
        refreshCachedRoomMetadata(id);
    }

    nhlog::db()->info("Restored {} rooms from cache (invites={}, joined={}, preview_rows={}, "
                      "models_initialized={})",
                      rowCount(),
                      inviteCount,
                      joinedRooms.size(),
                      previewedRooms.size(),
                      models.size());

    // Track unexpected eager materialization after metadata-only startup.
    startupMaterializationTrackingActive_ = true;

    endResetModel();

    const auto savedRoomId = UserSettings::instance()->currentRoomId();
    if (!savedRoomId.isEmpty() && cachedJoinedRooms_.contains(savedRoomId))
        setCurrentRoom(savedRoomId);

#ifdef KOMAI_DBUS_SYS
    setDbusInterfaceEnabled(MainWindow::instance()->dbusAvailable());
#endif
}
