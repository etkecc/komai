// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "RoomSettings.h"

#include <utility>

#include "timeline/RoomlistModel.h"

RoomSettingsAllowedRoomsModel::RoomSettingsAllowedRoomsModel(RoomSettings *parent)
  : QAbstractListModel(parent)
  , settings(parent)
{
    this->allowedRoomIds = settings->allowedRooms();

    for (const auto &parentRoomId : settings->parentSpaceRoomIds())
        this->parentSpaces.insert(parentRoomId);

    this->listedRoomIds = QStringList(parentSpaces.begin(), parentSpaces.end());

    for (const auto &e : std::as_const(this->allowedRoomIds)) {
        if (!this->parentSpaces.count(e))
            this->listedRoomIds.push_back(e);
    }
}

QHash<int, QByteArray>
RoomSettingsAllowedRoomsModel::roleNames() const
{
    return {
      {Roles::Name, "name"},
      {Roles::IsAllowed, "allowed"},
      {Roles::IsSpaceParent, "isParent"},
    };
}

int
RoomSettingsAllowedRoomsModel::rowCount(const QModelIndex &) const
{
    return listedRoomIds.size();
}

QVariant
RoomSettingsAllowedRoomsModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 || index.row() > listedRoomIds.size())
        return {};

    if (role == Roles::IsAllowed) {
        return allowedRoomIds.contains(listedRoomIds.at(index.row()));
    } else if (role == Roles::IsSpaceParent) {
        return parentSpaces.find(listedRoomIds.at(index.row())) != parentSpaces.cend();
    } else if (role == Roles::Name) {
        const auto id = listedRoomIds.at(index.row());
        if (const auto *roomModel = FilteredRoomlistModel::instance()) {
            const auto preview = roomModel->getRoomPreviewById(id);
            if (!preview.roomName().isEmpty())
                return preview.roomName();
        }

        return id;
    } else {
        return {};
    }
}

bool
RoomSettingsAllowedRoomsModel::setData(const QModelIndex &index, const QVariant &value, int role)
{
    if (index.row() < 0 || index.row() > listedRoomIds.size())
        return false;

    if (role != Roles::IsAllowed)
        return false;

    if (value.toBool()) {
        if (!allowedRoomIds.contains(listedRoomIds.at(index.row())))
            allowedRoomIds.push_back(listedRoomIds.at(index.row()));
    } else {
        allowedRoomIds.removeAll(listedRoomIds.at(index.row()));
    }

    return true;
}

void
RoomSettingsAllowedRoomsModel::addRoom(QString room)
{
    if (listedRoomIds.contains(room) || !room.startsWith('!'))
        return;

    beginInsertRows(QModelIndex(), listedRoomIds.size(), listedRoomIds.size());
    listedRoomIds.push_back(room);
    allowedRoomIds.push_back(room);
    endInsertRows();
}
