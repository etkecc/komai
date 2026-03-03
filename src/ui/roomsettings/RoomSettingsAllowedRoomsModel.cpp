// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "RoomSettings.h"

#include <utility>

#include "cache/Cache.h"

RoomSettingsAllowedRoomsModel::RoomSettingsAllowedRoomsModel(RoomSettings *parent)
  : QAbstractListModel(parent)
  , settings(parent)
{
    this->allowedRoomIds = settings->allowedRooms();

    auto prIds = cache::getParentRoomIds(settings->roomId().toStdString());
    for (const auto &prId : prIds) {
        this->parentSpaces.insert(QString::fromStdString(prId));
    }

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
        auto id   = listedRoomIds.at(index.row());
        auto info = cache::getRoomInfo({
          id.toStdString(),
        });
        if (!info.empty())
            return QString::fromStdString(info[id].name);
        else
            return "";
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
