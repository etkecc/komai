// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "RoomlistModel.h"

#include "logging/Logging.h"
#include "settings/ui/facade/UserSettingsPage.h"
#include "ui/MainWindow.h"

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
    Q_UNUSED(roomUpdate);
    refreshMatrixBackendRooms();
}

void
RoomlistModel::syncLeftRoom(const QString &qroomid)
{
    if (currentRoomPreview_ && currentRoomPreview_->roomid() == qroomid)
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
    Q_UNUSED(qroomid);
    nhlog::ui()->warn(
      "Ignoring legacy invite-side cache sync on the matrix-sdk migration branch");
}

void
RoomlistModel::sync(const komai::SyncUpdate &sync)
{
    Q_UNUSED(sync);
    refreshMatrixBackendRooms();
}

void
RoomlistModel::initializeRooms()
{
    const auto *mainWindow = MainWindow::instance();
    beginResetModel();
    resetRoomCollections(false);
    endResetModel();

    if (mainWindow && mainWindow->matrixBackendHandleId() != 0) {
        refreshMatrixBackendRooms();
    } else {
        nhlog::ui()->warn(
          "RoomlistModel initialization without an active matrix-sdk runtime is not supported "
          "on the migration branch");
    }

#ifdef KOMAI_DBUS_SYS
    setDbusInterfaceEnabled(MainWindow::instance()->dbusAvailable());
#endif
}
