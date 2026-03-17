// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

#include "UserProfile.h"

#include <algorithm>
#include <utility>

QHash<int, QByteArray>
DeviceInfoModel::roleNames() const
{
    return {
      {DeviceId, "deviceId"},
      {DeviceName, "deviceName"},
      {VerificationStatus, "verificationStatus"},
      {LastIp, "lastIp"},
      {LastTs, "lastTs"},
    };
}

QVariant
DeviceInfoModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= (int)deviceList_.size() || index.row() < 0)
        return {};

    switch (role) {
    case DeviceId:
        return deviceList_[index.row()].device_id;
    case DeviceName:
        return deviceList_[index.row()].display_name;
    case VerificationStatus:
        return QVariant::fromValue(deviceList_[index.row()].verification_status);
    case LastIp:
        return deviceList_[index.row()].lastIp;
    case LastTs:
        return deviceList_[index.row()].lastTs;
    default:
        return {};
    }
}

void
DeviceInfoModel::reset(const std::vector<DeviceInfo> &deviceList)
{
    beginResetModel();
    this->deviceList_ = std::move(deviceList);
    std::sort(this->deviceList_.begin(),
              this->deviceList_.end(),
              [](const DeviceInfo &a, const DeviceInfo &b) { return a.lastTs > b.lastTs; });
    endResetModel();
}

RoomInfoModel::RoomInfoModel(const std::map<std::string, RoomInfo> &raw, QObject *parent)
  : QAbstractListModel(parent)
{
    for (const auto &e : raw)
        roomInfos_.push_back(e);

    std::ranges::sort(roomInfos_,
                      [](const auto &a, const auto &b) { return a.second.name < b.second.name; });
}

QHash<int, QByteArray>
RoomInfoModel::roleNames() const
{
    return {
      {RoomId, "roomId"},
      {RoomName, "roomName"},
      {AvatarUrl, "avatarUrl"},
    };
}

QVariant
RoomInfoModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= (int)roomInfos_.size() || index.row() < 0)
        return {};

    switch (role) {
    case RoomId:
        return QString::fromStdString(roomInfos_[index.row()].first);
    case RoomName:
        return QString::fromStdString(roomInfos_[index.row()].second.name);
    case AvatarUrl:
        return QString::fromStdString(roomInfos_[index.row()].second.avatar_url);
    default:
        return {};
    }
}
